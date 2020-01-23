#define ARG0 rcx
#define ARG1 rdx
#define ARG2 r8
#define ARG3 r9
#define RET rax

.intel_syntax noprefix

// https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention?view=vs-2019
// TODO Save XMM6-XMM15.

.global _tina_swap
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
	
	sub rsp, 0xA0
	movaps [rsp + 0x90],  xmm6
	movaps [rsp + 0x80],  xmm7
	movaps [rsp + 0x70],  xmm8
	movaps [rsp + 0x60],  xmm9
	movaps [rsp + 0x50], xmm10
	movaps [rsp + 0x40], xmm11
	movaps [rsp + 0x30], xmm12
	movaps [rsp + 0x20], xmm13
	movaps [rsp + 0x10], xmm14
	movaps [rsp + 0x00], xmm15

	mov rax, rsp
	mov rsp, [ARG2]
	mov [ARG2], rax
	
	movaps  xmm6, [rsp + 0x90]
	movaps  xmm7, [rsp + 0x80]
	movaps  xmm8, [rsp + 0x70]
	movaps  xmm9, [rsp + 0x60]
	movaps xmm10, [rsp + 0x50]
	movaps xmm11, [rsp + 0x40]
	movaps xmm12, [rsp + 0x30]
	movaps xmm13, [rsp + 0x20]
	movaps xmm14, [rsp + 0x10]
	movaps xmm15, [rsp + 0x00]
	add rsp, 0xA0
	
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
