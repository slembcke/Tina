#define ARG0 rdi
#define ARG1 rsi
#define ARG2 rdx
#define ARG3 rcx
#define RET rax

.intel_syntax noprefix

.global tina_context
.func tina_context
tina_context: // (tina* coro, void** sp_loc, void* sp, tina_func* body) -> tina*
	// Save the caller's registers and stack pointer.
	// tina_yield() will restore them once the coroutine is primed.
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	mov [ARG1], rsp
	
	// save 'coro' and 'body' to preserved registers.
	mov rbp, ARG0
	mov rbx, ARG3
	
	// Align and apply the coroutine's stack.
	and ARG2, ~0xF
	mov rsp, ARG2
	
	// Push an NULL activation record onto the stack to make debuggers happy.
	push 0
	push 0
	
	// The coroutine is now primed. Yield the coroutine pointer back to tina_init().
	mov ARG1, ARG0
	call tina_yield
	
	// Pass the initial tina_yield() value on to body().
	mov ARG0, rbp
	mov ARG1, RET
	call rbx
	
	// Tail call tina_finish() with the final value from body().
	mov ARG0, rbp
	mov ARG1, RET
	jmp tina_finish
.endfunc

.global tina_swap
.func tina_swap
tina_swap: // (tina* coro, uintptr_t value, void** sp)
	// Preserve calling coroutine's registers.
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	
	// Swap stacks.
	mov rax, rsp
	mov rsp, [ARG2]
	mov [ARG2], rax
	
	// Restore callee coroutine's registers.
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	
	// return 'value' to the callee.
	mov RET, ARG1
	ret
.endfunc
