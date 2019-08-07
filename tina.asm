extern _abort

; System V rgument order: RDI, RSI, RDX, RCX, R8, and R9
; return value: RAX

%define ARG0 rdi
%define ARG1 rsi
%define RET rax

global _tina_wrap
_tina_wrap: ; (tina* coro, tina_func* body)
	; Save 'coro' and 'body' to preserved registers.
	mov r12, ARG0
	mov r13, ARG1
	call _tina_yield
	
	; Run the coroutine body.
	mov ARG0, r12
	mov ARG1, RET
	call r13
	
	; Yield the coroutine's return value.
	mov ARG0, r12
	mov ARG1, RET
	call _tina_yield
	
	; Crash if the coroutine is resumed after exiting.
	jmp _abort

global _tina_resume
global _tina_yield
_tina_yield: ; (tina* coro, uintptr_t value)
_tina_resume: ; (tina* coro, uintptr_t value)
	; Preserve calling coroutine registers.
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
	
	; Restore callee coroutine registers.
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	
	; Move 'value' to return register, pop and jump to the resume address.
	mov RET, ARG1
	ret
