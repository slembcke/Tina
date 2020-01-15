#include "stdbool.h"
#include "stdlib.h"
#include "tina.h"

tina_err_func* tina_err;

// Wrapper function for all coroutines that handles resuming dead coroutines.
uintptr_t tina_wrap(tina* coro, uintptr_t value){
	// tina_init() yields once so tina_wrap() can prep the stack and be ready to call into body().
	tina_swap(coro, value);
	
	// Call the body function and yield the final return value.
	tina_func* body = (tina_func*)value;
	tina_swap(coro, body(coro, value));
	
	// Any attempt to resume the coroutine after it's dead should call the error func.
	while(true){
		tina_err("Tina error: Attempted to resume a dead coroutine.");
		tina_swap(coro, 0);
	}
}

void* tina_init_stack(void* rsp);

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx){
	tina* coro = buffer;
	coro->ctx = ctx;
	
	coro->_rsp = tina_init_stack(buffer + size);
	
	// Allow tina_wrap() to finish initializing the stack.
	tina_swap(coro, (uintptr_t)body);
	return coro;
}
