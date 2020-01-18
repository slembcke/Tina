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
	
	// Private implementation details.
	void* _sp;
};

// Initialize a coroutine and return a pointer to it.
tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data);

// Yield execution to a coroutine.
uintptr_t tina_yield(tina* coro, uintptr_t value);

#ifdef TINA_IMPLEMENTATION

// TODO: Are there any relevant ABIs that aren't 16 byte aligned, downward moving stacks?
// TODO: Is it worthwhile to try and detect stack overflows?

tina* tina_init_stack(tina* coro, tina_func* body, void** sp_loc, void* sp);
uintptr_t tina_swap(tina* coro, uintptr_t value, void** sp);

tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data){
	tina* coro = buffer;
	(*coro) = (tina){.user_data = user_data, .running = true};
	return tina_init_stack(coro, body, &coro->_sp, buffer + size);
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	return tina_swap(coro, value, &coro->_sp);
}

void tina_context(tina* coro, tina_func* body){
	// Yield back to the tina_init_stack() call, and return the coroutine.
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

#if __amd64__ && __GNUC__
	#if __unix__
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

		asm(TINA_SYMBOL(tina_init_stack:));
		// Save the caller's registers and stack pointer.
		// tina_yield() will restore them once the coroutine is primed.
		asm("  push rbp");
		asm("  push rbx");
		asm("  push r12");
		asm("  push r13");
		asm("  push r14");
		asm("  push r15");
		asm("  mov ["ARG2"], rsp");
		// Align and apply the coroutine's stack.
		asm("  and "ARG3", ~0xF");
		asm("  mov rsp, "ARG3"");
		// Now executing within the new coroutine's stack!
		// When tina_context() first calls tina_yield() it will
		// return back to where tina_init_stack() was called.

		// Tail call tina_context() to finish the coroutine init.
		// The NULL activation record keeps the stack aligned and stack traces happy.
		asm("  push 0");
		asm("  jmp " TINA_SYMBOL(tina_context));
		
		asm(TINA_SYMBOL(tina_swap:));
		// Preserve calling coroutine's registers.
		asm("  push rbp");
		asm("  push rbx");
		asm("  push r12");
		asm("  push r13");
		asm("  push r14");
		asm("  push r15");
		// Swap stacks.
		asm("  mov rax, rsp");
		asm("  mov rsp, ["ARG2"]");
		asm("  mov ["ARG2"], rax");
		// Restore callee coroutine's registers.
		asm("  pop r15");
		asm("  pop r14");
		asm("  pop r13");
		asm("  pop r12");
		asm("  pop rbx");
		asm("  pop rbp");
		// return 'value' to the callee.
		asm("  mov "RET", "ARG1"");
		asm("  ret");

		asm(".att_syntax");
	#elif __win64__
		#error NYI
	#endif
#elif __ARM_EABI__ && __GNUC__
	// TODO: Is this an appropriate macro check for a 32 bit ARM ABI?
	// TODO: Only tested on RPi3.
	
	// NOTE: Code structure is nearly identical to the fully commented amd64 version.
	asm("tina_init_stack:");
	asm("  push {r4-r11, lr}");
	asm("  str sp, [r2]");
	asm("  and r3, r3, #~0xF");
	asm("  mov sp, r3");
	asm("  mov lr, #0");
	asm("  b tina_context");
	
	asm("tina_swap:");
	asm("  push {r4-r11, lr}");
	asm("  mov r3, sp");
	asm("  ldr sp, [r2]");
	asm("  str r3, [r2]");
	asm("  pop {r4-r11, lr}");
	asm("  mov r0, r1");
	asm("  bx lr");
#else
	#error Unknown CPU/compiler combo.
#endif

#endif
#endif
