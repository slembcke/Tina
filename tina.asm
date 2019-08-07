extern _tina_err
extern _abort

; System V rgument order: RDI, RSI, RDX, RCX, R8, and R9
; return value: RAX
%define ARG0 rdi
%define ARG1 rsi
%define RET rax

global _tina_wrap
_tina_wrap: ; (tina* coro, tina_func* body)
%push
%define %$coro r12
%define %$body r13

	; Save 'coro' and 'body' to preserved registers.
	mov %$coro, ARG0
	mov %$body, ARG1
	call _tina_yield
	
	; Run the coroutine body.
	mov ARG0, %$coro
	mov ARG1, RET
	call %$body
	
	; Yield the coroutine's return value.
	mov ARG0, %$coro
	mov ARG1, RET
	call _tina_yield
	
	; Crash if the coroutine is resumed after exiting.
	mov rax, [rel _tina_err]
	test rax, rax
	jz .abort
		lea ARG0, [rel .err_complete]
		call rax
	.abort: call _abort
	
	.err_complete: db "Attempted to resume a completed coroutine.",0
%pop

global _tina_resume, _tina_yield
_tina_resume: ; (tina* coro, uintptr_t value)
_tina_yield:
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
