#include "stdbool.h"
#include "stdlib.h"
#include "tina.h"

// TODO: Are there any relevant ABIs that aren't 16 byte aligned, downward moving stacks?
// TODO: Is it worthwhile to try and detect stack overflows?

// Defined in assembly.
tina* tina_context(tina* coro, void** sp_loc, void* sp, tina_func* body);
uintptr_t tina_swap(tina* coro, uintptr_t value, void** sp);

tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data){
	tina* coro = buffer;
	(*coro) = (tina){.user_data = user_data, .running = true};
	return tina_context(coro, &coro->_sp, buffer + size, body);
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	return tina_swap(coro, value, &coro->_sp);
}

// Function called when a coroutine body function exits.
void tina_finish(tina* coro, uintptr_t value){
	coro->running = false;
	tina_yield(coro, value);
	
	// Any attempt to resume the coroutine after it's dead should call the error func.
	while(true){
		if(coro->error_handler) coro->error_handler(coro, "Attempted to resume a dead coroutine.");
		tina_yield(coro, 0);
	}
}

asm(".intel_syntax noprefix");

#define ARG0 "rdi"
#define ARG1 "rsi"
#define ARG2 "rdx"
#define ARG3 "rcx"
#define RET "rax"

asm(".global tina_context");
asm(".func tina_context");
asm("tina_context:");
// Save the caller's registers and stack pointer.
// tina_yield() will restore them once the coroutine is primed.
asm("  push rbp");
asm("  push rbx");
asm("  push r12");
asm("  push r13");
asm("  push r14");
asm("  push r15");
asm("  mov ["ARG1"], rsp");
// save 'coro' and 'body' to preserved registers.
asm("  mov rbp, "ARG0"");
asm("  mov rbx, "ARG3"");
// Align and apply the coroutine's stack.
asm("  and "ARG2", ~0xF");
asm("  mov rsp, "ARG2"");
// Push an NULL activation record onto the stack to make debuggers happy.
asm("  push 0");
asm("  push 0");
// The coroutine is now primed. Yield the coroutine pointer back to tina_init().
asm("  mov "ARG1", "ARG0"");
asm("  call tina_yield");
// Pass the initial tina_yield() value on to body().
asm("  mov "ARG0", rbp");
asm("  mov "ARG1", "RET"");
asm("  call rbx");
// Tail call tina_finish() with the final value from body().
asm("  mov "ARG0", rbp");
asm("  mov "ARG1", "RET"");
asm("  jmp tina_finish");
asm(".endfunc");

asm(".global tina_swap");
asm(".func tina_swap");
asm("tina_swap:");
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
asm(".endfunc");

asm(".att_syntax");
