extern _abort

common _tina_err 8

; System V rgument order: RDI, RSI, RDX, RCX, R8, and R9
; return value: RAX
%define ARG0 rdi
%define ARG1 rsi
%define ARG2 rdx
%define ARG3 rcx
%define RET rax

tina_wrap: ; (tina* coro, tina_func* body)
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

global _tina_init
_tina_init: ; (void* buffer, size_t size, tina_func* body, void* ctx) -> tina*
%push
%define %$coro ARG0
%define %$size ARG1
%define %$body ARG2
%define %$ctx ARG3
	push rbp
	mov rbp, rsp
	
	; coro->ctx = ctx
	mov [%$coro + 0], %$ctx
	
	; Calculate and align the stack top.
	lea rsp, [%$coro + %$size]
	and rsp, ~0xF
	; Push tina_wrap()
	lea rax, [rel tina_wrap]
	push rax
	; Save space for the preserved registers.
	sub rsp, 6*8
	; coro->_rsp = rsp
	mov [%$coro + 8], rsp
	mov rsp, rbp
	
	; Start the coroutine, pass body() to tina_wrap().
	mov ARG1, %$body
	call _tina_resume
	
	mov RET, %$coro
	pop rbp
	ret
%pop

global _tina_resume, _tina_yield
_tina_resume: ; (tina* coro, uintptr_t value)
_tina_yield:
%push
%define %$coro ARG0
%define %$value ARG1
	; Preserve calling coroutine's registers.
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
	
	; Swap stacks.
	mov rdx, rsp
	mov rsp, [%$coro + 8]
	mov [%$coro + 8], rdx
	
	; Restore callee coroutine's registers.
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
	
	; Move 'value' to return register, pop and jump to the resume address.
	mov RET, %$value
	ret
%pop
