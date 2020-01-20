#define ARG0 rcx
#define ARG1 rdx
#define ARG2 r8
#define ARG3 r9
#define RET rax

.intel_syntax

_tina_swap:
	push rbp
	push rbx
	push rsi
	push rdi
	push r12
	push r13
	push r14
	push r15
	push gs:0x8
	push gs:0x10
	push gs:0x1478
	mov rax, rsp
	mov rsp, [ARG2]
	mov [ARG2], rax
	pop gs:0x1478
	pop gs:0x10
	pop gs:0x8
	pop r15
	pop r14
	pop r13
	pop r12
	pop rdi
	pop rsi
	pop rbx
	pop rbp
	mov RET, ARG1
	ret
