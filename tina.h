#ifndef TINA_H
#define TINA_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

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
	// Is the coroutine still running. (read only)
	bool running;
	// Pointer to the coroutine's memory buffer.
	void* buffer;
	
	// Private implementation details.
	void* _sp;
};

// Initialize a coroutine and return a pointer to it.
// Coroutine's created this way do not need to be destroyed or freed.
// You are responsible for 'buffer', but it will be stored in tina->buffer for you.
tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data);

// Allocate and initialize a coroutine and return a pointer to it.
tina* tina_new(size_t size, tina_func* body, void* user_data);
// Free a coroutine created by tina_new().
void tina_free(tina* coro);

// Yield execution to a coroutine.
uintptr_t tina_yield(tina* coro, uintptr_t value);

#ifdef TINA_IMPLEMENTATION

// TODO: Are there any relevant ABIs that aren't 16 byte aligned, downward moving stacks?
// TODO: Is it worthwhile to try and detect stack overflows?

// Types for the assembly functions.
typedef tina* _tina_init_stack_f(tina* coro, tina_func* body, void** sp_loc, void* sp);
typedef uintptr_t _tina_swap_f(tina* coro, uintptr_t value, void** sp);

// Symbols for the assembly functions.
// These are either defined as inline assembly (GCC/Clang) of binary blobs (MSVC).
extern const uint64_t _tina_init_stack[];
extern const uint64_t _tina_swap[];

// Macros to make calling the functions slightly cleaner.
#define TINA_INIT_STACK ((_tina_init_stack_f*)(void*)_tina_init_stack)
#define TINA_SWAP ((_tina_swap_f*)(void*)_tina_swap)

tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data) {
	tina* coro = (tina*)buffer;
	coro->user_data = user_data;
	coro->running = true;
	return TINA_INIT_STACK(coro, body, &coro->_sp, (uint8_t*)buffer + size);
}

tina* tina_new(size_t size, tina_func* body, void* user_data){
	return tina_init(malloc(size), size, body, user_data);
}

void tina_free(tina* coro){
	free(coro->buffer);
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	return TINA_SWAP(coro, value, &coro->_sp);
}

void _tina_context(tina* coro, tina_func* body){
	// Yield back to the _tina_init_stack() call, and return the coroutine.
	uintptr_t value = tina_yield(coro, (uintptr_t)coro);
	// Call the body function with the first value.
	value = body(coro, value);
	// body() has exited, and the coroutine is finished.
	coro->running = false;
	// Yield the final return value back to the calling thread.
	tina_yield(coro, value);
	
	// Any attempt to resume the coroutine after it's finished should call the error func.
	while(true){
		if(coro->error_handler) coro->error_handler(coro, "Attempted to resume a dead coroutine.");
		tina_yield(coro, 0);
	}
}

#if __ARM_EABI__ && __GNUC__
	// TODO: Is this an appropriate macro check for a 32 bit ARM ABI?
	// TODO: Only tested on RPi3.
	
	// Since the arm version is by far the shortest, I'll document this one.
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
	#if __APPLE__
		#define TINA_SYMBOL(sym) "_"#sym
	#else
		#define TINA_SYMBOL(sym) #sym
	#endif
	
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
	asm("  mov ["ARG2"], rsp");
	asm("  and "ARG3", ~0xF");
	asm("  mov rsp, "ARG3);
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
	asm("  mov rsp, ["ARG2"]");
	asm("  mov ["ARG2"], rax");
	asm("  pop r15");
	asm("  pop r14");
	asm("  pop r13");
	asm("  pop r12");
	asm("  pop rbx");
	asm("  pop rbp");
	asm("  mov "RET", "ARG1);
	asm("  ret");
	
	asm(".att_syntax");
#elif __WIN64__
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
		0xb848909090909000, (uint64_t)_tina_context,
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
#endif

#endif // TINA_IMPLEMENTATION
#endif // TINA_H
