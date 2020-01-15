%ifdef __linux__
	%define SYSTEM_V_ABI
%elifdef MACOS
	%define SYSTEM_V_ABI

	%define tina_init _tina_init
	%define tina_resume _tina_resume
	%define tina_yield _tina_yield
%else
	%define SYSTEM_V_ABI
	; %error No platform defined.
%endif

%ifdef SYSTEM_V_ABI
	%define ARG0 rdi
	%define ARG1 rsi
	%define ARG2 rdx
	%define ARG3 rcx
	%define RET rax
%else
	%error No ABI defined.
%endif

section .data

global tina_err
tina_err: dq 0

section .text

tina_wrap: ; (tina* coro, tina_func* body)
%push
%define %$coro r12
%define %$body r13

	; Save 'coro' and 'body' to preserved registers.
	mov %$coro, ARG0
	mov %$body, ARG1
	call tina_yield
	
	; Run the coroutine body.
	mov ARG0, %$coro
	mov ARG1, RET
	call %$body
	
	; Yield the coroutine's return value.
	mov ARG0, %$coro
	mov ARG1, RET
	call tina_yield
	
	; Call the error function in an endless loop if attempting to resume it again.
	err:
	lea ARG0, [rel .err_complete]
	call [rel tina_err]
	
	mov ARG0, %$coro
	mov ARG1, 0
	call tina_yield
	jmp err
	
	.err_complete: db "Attempted to resume a completed coroutine.", 0
%pop

global tina_init
tina_init: ; (void* buffer, size_t size, tina_func* body, void* ctx) -> tina*
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
	call tina_resume
	
	mov RET, %$coro
	pop rbp
	ret
%pop

global tina_resume, tina_yield
tina_resume: ; (tina* coro, uintptr_t value)
tina_yield:
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
