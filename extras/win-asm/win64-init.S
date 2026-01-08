// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Scott Lembcke and Howling Moon Software

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

	mov [ARG1], rsp
	and ARG2, -16
	mov rsp, ARG2
	mov gs:0x1478, ARG0
	mov gs:0x10, ARG0
	mov gs:0x8, ARG2
	
	// Push stack frame sentinel.
	push 0
	// Reserve shadow space.
	sub rsp, 0x20
	
	// Pad the binary so the jump address will be 8 byte aligned and can be plopped into the array as data.
	nop
	mov rax, 0xDEADBEEFDEADBEEF
	jmp rax
