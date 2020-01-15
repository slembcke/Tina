; TODO why did this stop working?
%ifdef __linux__
	%define SYSTEM_V_ABI
%elifdef MACOS
	%define SYSTEM_V_ABI

	%define tina_init _tina_init
	%define tina_swap _tina_swap
	%define tina_swap _tina_swap
%else
	; %error No platform defined.
	%define SYSTEM_V_ABI
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

extern tina_wrap
extern tina_err

section .text

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
	call tina_swap
	
	mov RET, %$coro
	pop rbp
	ret
%pop

global tina_swap, tina_swap
tina_swap: ; (tina* coro, uintptr_t value)
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
