// NOTE: This is a nearly line for line identical to the amd64 version which is commented.

.global tina_context
.func tina_context
tina_context:
	push {r4-r11, lr}
	str sp, [r1]
	
	mov r4, r0
	mov r5, r3
	
	and r2, r2, #~0xF
	mov sp, r2
	
	mov r2, #0
	push {r2}
	push {r2}
	
	mov r1, r0
	bl tina_yield
	
	mov r1, r0
	mov r0, r4
	blx r5
	
	mov r1, r0
	mov r0, r4
	b tina_finish
.endfunc

.global tina_swap
.func tina_swap
tina_swap:
	push {r4-r11, lr}
	
	mov r3, sp
	ldr sp, [r2]
	str r3, [r2]
	
	pop {r4-r11, lr}
	mov r0, r1
	bx lr
.endfunc
