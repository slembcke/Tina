#include "stdbool.h"
#include "stdlib.h"
#include "tina.h"

// Wrapper function for all coroutines that handles resuming dead coroutines.
static uintptr_t tina_wrap(tina* coro, uintptr_t value){
	tina_yield(coro, value);
	
	// Call the body function and yield the final return value.
	tina_func* body = value;
	tina_yield(coro, body(coro, value));
	
	// Any attempt to resume the coroutine after it's dead should call the error func.
	while(true){
		tina_err("Tina error: Attempted to resume a dead coroutine.");
		tina_yield(0);
	}
}

void* tina_init_stack(tina_func* wrap);

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx){
	tina* coro = buffer;
	coro->ctx = ctx;
	
	void* rsp = (uintptr_t)(buffer + size) & ~0xF;
	coro->_rsp = tina_init_stack(tina_wrap);
	
	// Allow tina_wrap() to finish initializing the stack.
	tina_resume(coro, body);
}
