.weak tina_finish

.global tina_init_stack
tina_init_stack: # (tina* coro, void** sp_loc, void* sp, tina_func* body) -> tina*
	# Push all the saved registers, and save the stack pointer in the coroutine.
	# tina_yield() below will restore these and return from tina_stack_init.
	push {r4-r11, lr}
	str sp, [r1]
	
	# save 'coro' and 'body' to preserved registers.
	mov r4, r0
	mov r5, r3
	
	# Align and apply the coroutine's stack.
	and r2, r2, #~0xF
	mov sp, r2
	
	# Yield back to, and return 'coro' from tina_stack_init().
	mov r1, r0
	bl tina_yield
	
	# Pass the initial tina_yield() value on to body().
	mov r1, r0
	mov r0, r4
	blx r5
	
	# Pass the final return value to tina_finish().
	mov r1, r0
	mov r0, r4
	b tina_finish

.global tina_swap
tina_swap: #(tina* coro, uintptr_t value, void** sp) -> uintptr_t value
	push {r4-r11, lr}
	
	mov r3, sp
	ldr sp, [r2]
	str r3, [r2]
	
	pop {r4-r11, lr}
	mov r0, r1
	bx lr
