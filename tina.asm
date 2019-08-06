extern _tina_yield
extern _abort

; System V rgument order: RDI, RSI, RDX, RCX, R8, and R9
; return value: RAX

global _tina_catch
_tina_catch:
	popcall_rdx
	call _tina_yield
	
	; Crash if the coroutine is resumed after exiting.
	jmp _abort

global _tina_resume
_tina_resume: ; (tina* coro, uintptr_t value)
	push rbp
	; TODO need to save rsp to coro->_rsp_yield
	
	; Restore the coroutine's stack.
	mov rsp, [rdi + 8]
	; Move 'value' to return value, pop and call the resume address.
	mov rax, rsi
	pop rdx
	call rdx
	
	; leave
	pop rbp
	ret

