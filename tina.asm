extern _abort

; System V rgument order: RDI, RSI, RDX, RCX, R8, and R9
; return value: RAX

%define ARG0 rdi
%define ARG1 rsi
%define RET rax

%macro push_regs 0
	push rbp
	push rbx
	push r12
	push r13
	push r14
	push r15
%endmacro

%macro pop_regs 0
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
	pop rbp
%endmacro

%macro swap_rsp 1
	mov rdx, rsp
	mov rsp, [%1 + 8]
	mov [%1 + 8], rdx
%endmacro

global _tina_wrap
_tina_wrap: ; (tina* coro, tina_func* body)
	; Push body() to the stack and yield.
	mov rbp, rsp
	push ARG0
	push ARG1
	call _tina_yield
	
	; Run the coroutine body.
	mov ARG0, [rbp - 8]
	mov ARG1, RET
	call [rbp - 16]
	
	; Yield the coroutine's return value.
	mov ARG0, [rbp - 8]
	mov ARG1, RET
	call _tina_yield
	
	; Crash if the coroutine is resumed after exiting.
	jmp _abort

global _tina_resume
global _tina_yield
_tina_yield: ; (tina* coro, uintptr_t value)
_tina_resume: ; (tina* coro, uintptr_t value)
	push_regs
	swap_rsp ARG0
	pop_regs
	
	; Move 'value' to return register, pop and jump to the resume address.
	mov RET, ARG1
	pop rdx
	jmp rdx
