%define ARG0 rdi
%define ARG1 rsi
%define ARG2 rdx
%define ARG3 rcx
%define RET rax

extern tina_finish
extern tina_yield

global tina_init_stack
tina_init_stack: ; (tina* coro, void** sp_loc, void* sp, tina_func* body) -> tina*
	; Push all the saved registers, and save the stack pointer.
	; tina_yield() below will restore these and return from tina_stack_init.
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	mov [ARG1], rsp
	
	; save 'coro' and 'body' to preserved registers.
	mov rbp, ARG0
	mov rbx, ARG3
	
	; Align and apply the coroutine's stack.
	and ARG2, ~0xF
	mov rsp, ARG2
	
	; Push an NULL activation record onto the stack to make debuggers happy.
	push 0
	push 0
	
	; Yield back to, and return 'coro' from tina_stack_init().
	mov ARG1, ARG0
	call tina_yield
	
	; Pass the initial tina_yield() value on to body().
	mov ARG0, rbp
	mov ARG1, RET
	call rbx
	
	; Tail call tina_finish() with the final value from body().
	mov ARG0, rbp
	mov ARG1, RET
	jmp tina_finish

global tina_swap
tina_swap: ; (tina* coro, uintptr_t value, void** sp)
	; Preserve calling coroutine's registers.
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	
	; Swap stacks.
	mov rax, rsp
	mov rsp, [ARG2]
	mov [ARG2], rax
	
	; Restore callee coroutine's registers.
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	
	mov RET, ARG1
	ret
