// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Scott Lembcke and Howling Moon Software

#define ARG0 rcx
#define ARG1 rdx
#define ARG2 r8
#define ARG3 r9
#define RET rax

.intel_syntax noprefix

// https://docs.microsoft.com/en-us/cpp/build/x64-calling-convention?view=vs-2019
// https://en.wikipedia.org/wiki/Win32_Thread_Information_Block#Contents_of_the_TIB_on_Windows
// https://github.com/wine-mirror/wine/blob/1aff1e6a370ee8c0213a0fd4b220d121da8527aa/include/winternl.h#L271
// https://github.com/boostorg/context/tree/develop/src/asm

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
	push gs:0x8 // Stack base (high address)
	push gs:0x10 // Stack limit (low address)
	push gs:0x1478 // Deallocation stack (also low address)
	
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

	mov [ARG0], rsp
	mov rsp, [ARG1]
	
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
	mov RET, ARG2
	ret
