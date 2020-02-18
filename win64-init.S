#define ARG0 rcx
#define ARG1 rdx
#define ARG2 r8
#define ARG3 r9
#define RET rax

.intel_syntax noprefix

.global _tina_init_stack
_tina_init_stack:
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

	mov [ARG2], rsp
	and ARG3, ~0xF
	mov rsp, ARG3
	// https://en.wikipedia.org/wiki/Win32_Thread_Information_Block#Contents_of_the_TIB_on_Windows
	// https://github.com/wine-mirror/wine/blob/1aff1e6a370ee8c0213a0fd4b220d121da8527aa/include/winternl.h#L271
	// https://github.com/boostorg/context/tree/develop/src/asm
	mov gs:0x1478, ARG0 // Deallocation stack (also low address)
	mov gs:0x10, ARG0 // Stack limit (low address)
	mov gs:0x8, ARG3 // Stack base (high address)
	
	// Push stack frame sentinel.
	push 0
	// Reserve shadow space.
	sub rsp, 0x20
	
	// Pad the binary so the jump address will be 8 byte aligned and can be plopped into the array as data.
	nop
	mov rax, 0xDEADBEEFDEADBEEF
	jmp rax
