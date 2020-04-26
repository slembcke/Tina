/*
Copyright (c) 2019 Scott Lembcke

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef TINA_H
#define TINA_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Coroutine type.
typedef struct tina tina;

// Coroutine body function type.
typedef uintptr_t tina_func(tina* coro, uintptr_t value);

// Error callback function type.
typedef void tina_error_handler(tina* coro, const char* message);

struct tina {
	// User defined context pointer.
	void* user_data;
	// User defined name. (optional)
	const char* name;
	// User defined error handler. (optional)
	tina_error_handler* error_handler;
	// Pointer to the coroutine's memory buffer. (readonly)
	void* buffer;
	// Size of the buffer. (readonly)
	size_t size;
	// Has the coroutine's function exited? (readonly)
	bool completed;
	
	// Private implementation details.
	void* _sp;
	uint32_t _magic;
};

// Initialize a coroutine and return a pointer to it.
// Coroutine's created this way do not need to be destroyed or freed.
// You are responsible for 'buffer', but it will be stored in tina->buffer for you.
tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data);

// Allocate and initialize a coroutine and return a pointer to it.
tina* tina_new(size_t size, tina_func* body, void* user_data);
// Free a coroutine created by tina_new().
void tina_free(tina* coro);

// Yield execution into (or out of) a coroutine.
// NOTE: The implementation is simplistic and just swaps a continuation stored in the coroutine.
// In other words: Coroutines can yield to other coroutines, but don't yield into a coroutine that hasn't yielded back to it's caller yet.
// Treat them as reentrant or you'll get continuations and coroutines scrambled in a way that's probably more confusing than helpful.
uintptr_t tina_yield(tina* coro, uintptr_t value);

#ifdef TINA_IMPLEMENTATION

#ifndef _TINA_ASSERT
#include <stdio.h>
#define _TINA_ASSERT(_COND_, _MESSAGE_) { if(!(_COND_)){fprintf(stdout, _MESSAGE_"\n"); abort();} }
#endif

// Magic number to help assert for memory corruption errors.
#define _TINA_MAGIC 0x54494E41u

// Symbols for the assembly functions.
// These are either defined as inline assembly (GCC/Clang) of binary blobs (MSVC).
extern const uint64_t _tina_swap[];
extern const uint64_t _tina_init_stack[];

tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data) {
	tina* coro = (tina*)buffer;
	coro->user_data = user_data;
	coro->completed = false;
	coro->buffer = buffer;
	coro->size = size;
	coro->_magic = _TINA_MAGIC;

	typedef tina* init_func(tina* coro, tina_func* body, void** sp_loc, void* sp);
	init_func* init = ((init_func*)(void*)_tina_init_stack);
	return init(coro, body, &coro->_sp, (uint8_t*)buffer + size);
}

tina* tina_new(size_t size, tina_func* body, void* user_data){
	return tina_init(malloc(size), size, body, user_data);
}

void tina_free(tina* coro){
	free(coro->buffer);
}

void _tina_context(tina* coro, tina_func* body){
	// Yield back to the _tina_init_stack() call, and return the coroutine.
	uintptr_t value = tina_yield(coro, (uintptr_t)coro);
	// Call the body function with the first value.
	value = body(coro, value);
	// body() has exited, and the coroutine is completed.
	coro->completed = true;
	// Yield the final return value back to the calling thread.
	tina_yield(coro, value);
	
	// Any attempt to resume the coroutine after it's completed should call the error func.
	while(true){
		if(coro->error_handler) coro->error_handler(coro, "Attempted to resume a dead coroutine.");
		tina_yield(coro, 0);
	}
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	_TINA_ASSERT(coro->_magic == _TINA_MAGIC, "Tina Error: Coroutine has likely had a stack overflow. Bad magic number detected.");
	
	typedef uintptr_t swap_func(tina* coro, uintptr_t value, void** sp);
	swap_func* swap = ((swap_func*)(void*)_tina_swap);
	// TODO swap no longer needs the coro pointer.
	// Could save a couple instructions? Meh. Too much testing effort.
	return swap(NULL, value, &coro->_sp);
}

#if __APPLE__
	#define TINA_SYMBOL(sym) "_"#sym
#else
	#define TINA_SYMBOL(sym) #sym
#endif

#if __ARM_EABI__ && __GNUC__
	// TODO: Is this an appropriate macro check for a 32 bit ARM ABI?
	// TODO: Only tested on RPi3.
	
	// Since the 32 bit ARM version is by far the shortest, I'll document this one.
	// The other variations are basically the same structurally.
	
	// _tina_init_stack() sets up the stack and initial execution of the coroutine.
	asm("_tina_init_stack:");
	// First things first, save the registers protected by the ABI
	asm("  push {r4-r11, lr}");
	asm("  vpush {q4-q7}");
	// Now store the stack pointer in the couroutine.
	// _tina_context() will call tina_yield() to restore the stack and registers later.
	asm("  str sp, [r2]");
	// Align the stack top to 16 bytes as requested by the ABI and set it to the stack pointer.
	asm("  and r3, r3, #~0xF");
	asm("  mov sp, r3");
	// Finally, tail call into _tina_context.
	// By setting the caller to null, debuggers will show _tina_context() as a base stack frame.
	asm("  mov lr, #0");
	asm("  b _tina_context");
	
	// https://static.docs.arm.com/ihi0042/g/aapcs32.pdf
	// _tina_swap() is responsible for swapping out the registers and stack pointer.
	asm("_tina_swap:");
	// Like above, save the ABI protected registers and save the stack pointer.
	asm("  push {r4-r11, lr}");
	asm("  vpush {q4-q7}");
	asm("  mov r3, sp");
	// Restore the stack pointer for the new coroutine.
	asm("  ldr sp, [r2]");
	// And save the previous stack pointer into the coroutine object.
	asm("  str r3, [r2]");
	// Restore the new coroutine's protected registers.
	asm("  vpop {q4-q7}");
	asm("  pop {r4-r11, lr}");
	// Move the 'value' parameter to the return value register.
	asm("  mov r0, r1");
	// And perform a normal return instruction.
	// This will return from tina_yield() in the new coroutine.
	asm("  bx lr");
#elif __amd64__ && (__unix__ || __APPLE__)
	#define ARG0 "rdi"
	#define ARG1 "rsi"
	#define ARG2 "rdx"
	#define ARG3 "rcx"
	#define RET "rax"
	
	asm(".intel_syntax noprefix");
	
	asm(TINA_SYMBOL(_tina_init_stack:));
	asm("  push rbp");
	asm("  push rbx");
	asm("  push r12");
	asm("  push r13");
	asm("  push r14");
	asm("  push r15");
	asm("  mov [" ARG2 "], rsp");
	asm("  and " ARG3 ", ~0xF");
	asm("  mov rsp, " ARG3);
	asm("  push 0");
	asm("  jmp " TINA_SYMBOL(_tina_context));
	
	// https://software.intel.com/sites/default/files/article/402129/mpx-linux64-abi.pdf
	asm(TINA_SYMBOL(_tina_swap:));
	asm("  push rbp");
	asm("  push rbx");
	asm("  push r12");
	asm("  push r13");
	asm("  push r14");
	asm("  push r15");
	asm("  mov rax, rsp");
	asm("  mov rsp, [" ARG2 "]");
	asm("  mov [" ARG2 "], rax");
	asm("  pop r15");
	asm("  pop r14");
	asm("  pop r13");
	asm("  pop r12");
	asm("  pop rbx");
	asm("  pop rbp");
	asm("  mov " RET ", " ARG1);
	asm("  ret");
	
	asm(".att_syntax");
#elif __WIN64__ || defined(_WIN64)
	// MSVC doesn't allow inline assembly, assemble to binary blob then.
	
	#if __GNUC__
		#define TINA_SECTION_ATTRIBUTE __attribute__((section(".text#")))
	#elif _MSC_VER
		#pragma section("tina", execute)
		#define TINA_SECTION_ATTRIBUTE __declspec(allocate("tina"))
	#else
		#error Unknown/untested compiler for Win64. 
	#endif
	
	// Assembled and dumped from win64-init.S
	TINA_SECTION_ATTRIBUTE
	const uint64_t _tina_init_stack[] = {
		0x5541544157565355, 0x2534ff6557415641,
		0x2534ff6500000008, 0x2534ff6500000010,
		0xa0ec814800001478, 0x9024b4290f000000,
		0x8024bc290f000000, 0x2444290f44000000,
		0x4460244c290f4470, 0x290f44502454290f,
		0x2464290f4440245c, 0x4420246c290f4430,
		0x290f44102474290f, 0xe18349208949243c,
		0x0c894865cc894cf0, 0x8948650000147825,
		0x4c6500000010250c, 0x6a00000008250c89,
		0xb8489020ec834800, (uint64_t)_tina_context,
		0x909090909090e0ff, 0x9090909090909090,
	};

	// Assembled and dumped from win64-swap.S
	TINA_SECTION_ATTRIBUTE
	const uint64_t _tina_swap[] = {
		0x5541544157565355, 0x2534ff6557415641,
		0x2534ff6500000008, 0x2534ff6500000010,
		0xa0ec814800001478, 0x9024b4290f000000,
		0x8024bc290f000000, 0x2444290f44000000,
		0x4460244c290f4470, 0x290f44502454290f,
		0x2464290f4440245c, 0x4420246c290f4430,
		0x290f44102474290f, 0x208b49e08948243c,
		0x9024b4280f008949, 0x8024bc280f000000,
		0x2444280f44000000, 0x4460244c280f4470,
		0x280f44502454280f, 0x2464280f4440245c,
		0x4420246c280f4430, 0x280f44102474280f,
		0x0000a0c48148243c, 0x00147825048f6500,
		0x00001025048f6500, 0x00000825048f6500,
		0x415d415e415f4100, 0xd089485d5b5e5f5c,
		0x90909090909090c3, 0x9090909090909090,
	};
#elif __aarch64__ && __GNUC__
	asm(TINA_SYMBOL(_tina_init_stack:));
	asm("  sub sp, sp, 0xA0");
	asm("  stp x19, x20, [sp, 0x00]");
	asm("  stp x21, x22, [sp, 0x10]");
	asm("  stp x23, x24, [sp, 0x20]");
	asm("  stp x25, x26, [sp, 0x30]");
	asm("  stp x27, x28, [sp, 0x40]");
	asm("  stp x29, x30, [sp, 0x50]");
	asm("  stp d8 , d9 , [sp, 0x60]");
	asm("  stp d10, d11, [sp, 0x70]");
	asm("  stp d12, d13, [sp, 0x80]");
	asm("  stp d14, d15, [sp, 0x90]");
	asm("  mov x4, sp");
	asm("  str x4, [x2]");
	asm("  and x3, x3, #~0xF");
	asm("  mov sp, x3");
	asm("  mov lr, #0");
	asm("  b _tina_context");

	asm(TINA_SYMBOL(_tina_swap:));
	asm("  sub sp, sp, 0xA0");
	asm("  stp x19, x20, [sp, 0x00]");
	asm("  stp x21, x22, [sp, 0x10]");
	asm("  stp x23, x24, [sp, 0x20]");
	asm("  stp x25, x26, [sp, 0x30]");
	asm("  stp x27, x28, [sp, 0x40]");
	asm("  stp x29, x30, [sp, 0x50]");
	asm("  stp d8 , d9 , [sp, 0x60]");
	asm("  stp d10, d11, [sp, 0x70]");
	asm("  stp d12, d13, [sp, 0x80]");
	asm("  stp d14, d15, [sp, 0x90]");
	asm("  mov x3, sp");
	asm("  ldr x4, [x2]");
	asm("  mov sp, x4");
	asm("  str x3, [x2]");
	asm("  ldp x19, x20, [sp, 0x00]");
	asm("  ldp x21, x22, [sp, 0x10]");
	asm("  ldp x23, x24, [sp, 0x20]");
	asm("  ldp x25, x26, [sp, 0x30]");
	asm("  ldp x27, x28, [sp, 0x40]");
	asm("  ldp x29, x30, [sp, 0x50]");
	asm("  ldp d8 , d9 , [sp, 0x60]");
	asm("  ldp d10, d11, [sp, 0x70]");
	asm("  ldp d12, d13, [sp, 0x80]");
	asm("  ldp d14, d15, [sp, 0x90]");
	asm("  add sp, sp, 0xA0");
	asm("  mov x0, x1");
	asm("  ret");
#endif

#endif // TINA_IMPLEMENTATION

#ifdef __cplusplus
}
#endif

#endif // TINA_H
