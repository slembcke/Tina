%define ARG0 rdi
%define ARG1 rsi
%define RET rax

extern tina_wrap
extern tina_err

global tina_init_stack
tina_init_stack: ; (void* rsp) -> void* rsp
%push
	push rbp
	mov rbp, rsp
	
	mov rsp, ARG0
	; Make sure the stack is aligned.
	and rsp, ~0xF
	
	; Push tina_wrap()
	lea rax, [rel tina_wrap]
	push rax
	
	; Save space for the registers that tina_swap() will pop.
	; They are unitialized and unused, but simpler than adding a special case.
	sub rsp, 6*8
	
	; Return the updated stack pointer.
	mov RET, rsp
	leave
	ret
%pop

global tina_swap, tina_swap
tina_swap: ; (tina* coro, uintptr_t value)
%push
	; Preserve calling coroutine's registers.
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	
	; Swap stacks.
	mov rdx, rsp
	mov rsp, [ARG0 + 8]
	mov [ARG0 + 8], rdx
	
	; Restore callee coroutine's registers.
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	
	; 'value' passed to the caller's tina_swap() should be returned from the callee's tina_swap() call.
	mov RET, ARG1
	; Because we swapped stacks, we will return from the callee's tina_swap() call, not the caller's.
	ret
%pop
