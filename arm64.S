.global _tina_init_stack
_tina_init_stack:
	sub sp, sp, 0xA0
	stp d8 , d9 , [sp, 0x00]
	stp d10, d11, [sp, 0x10]
	stp d12, d13, [sp, 0x20]
	stp d14, d15, [sp, 0x30]
	stp x19, x20, [sp, 0x40]
	stp x21, x22, [sp, 0x50]
	stp x23, x24, [sp, 0x60]
	stp x25, x26, [sp, 0x70]
	stp x27, x28, [sp, 0x80]
	stp x29, x30, [sp, 0x90]
	
	mov x4, sp
	str x4, [x2]
	
	and x3, x3, #~0xF
	mov sp, x3
	
	mov lr, #0
	b _tina_context
num: 

.global _tina_swap
_tina_swap:
	sub sp, sp, 0xA0
	stp d8 , d9 , [sp, 0x00]
	stp d10, d11, [sp, 0x10]
	stp d12, d13, [sp, 0x20]
	stp d14, d15, [sp, 0x30]
	stp x19, x20, [sp, 0x40]
	stp x21, x22, [sp, 0x50]
	stp x23, x24, [sp, 0x60]
	stp x25, x26, [sp, 0x70]
	stp x27, x28, [sp, 0x80]
	stp x29, x30, [sp, 0x90]
	
	mov x3, sp
	ldr x4, [x2]
	mov sp, x4
	str x3, [x2]
	
	ldp d8 , d9 , [sp, 0x00]
	ldp d10, d11, [sp, 0x10]
	ldp d12, d13, [sp, 0x20]
	ldp d14, d15, [sp, 0x30]
	ldp x19, x20, [sp, 0x40]
	ldp x21, x22, [sp, 0x50]
	ldp x23, x24, [sp, 0x60]
	ldp x25, x26, [sp, 0x70]
	ldp x27, x28, [sp, 0x80]
	ldp x29, x30, [sp, 0x90]
	add sp, sp, 0xA0
	
	mov x0, x1
	ret
