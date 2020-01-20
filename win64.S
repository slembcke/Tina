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
//	push gs:0x20
	mov [ARG2], rsp
	and ARG3, ~0xF
	mov rsp, ARG3
	// https://en.wikipedia.org/wiki/Win32_Thread_Information_Block#Contents_of_the_TIB_on_Windows
	// https://github.com/septag/deboost.context/blob/master/asm/jump_x86_64_ms_pe_gas.asm
	// https://github.com/wine-mirror/wine/blob/1aff1e6a370ee8c0213a0fd4b220d121da8527aa/include/winternl.h#L271
	mov gs:0x8, ARG3 // Stack base (high address)
	mov gs:0x10, ARG0 // Stack limit (low address)
	mov gs:0x1478, ARG0 // Deallocation stack (also low address)
	// boost.context left the pop location for this uninitialized (or zeroed)? I think...
	// Also it used 0x18 which seems wrong from the docs?
//	mov gs:0x20, ARG0);
	push 0
	nop
	nop
	nop
	mov rax, 0xDEADBEEFDEADBEEF
	jmp rax

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
