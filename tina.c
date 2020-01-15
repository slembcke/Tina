#include "stdbool.h"
#include "stdlib.h"
#include "tina.h"

#include "stdio.h"

tina_err_func* tina_err;

// Wrapper function for all coroutines that handles resuming dead coroutines.
uintptr_t tina_wrap(tina* coro, uintptr_t value){
	// tina_init() yields once so tina_wrap() can prep the stack and be ready to call into body().
	tina_swap(coro, value);
	
	// Call the body function and yield the final return value.
	tina_func* body = value;
	tina_swap(coro, body(coro, value));
	
	// Any attempt to resume the coroutine after it's dead should call the error func.
	while(true){
		tina_err("Tina error: Attempted to resume a dead coroutine.");
		tina_swap(coro, 0);
	}
}

// void* tina_init_stack(tina_func* wrap);

// tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx){
// 	tina* coro = buffer;
// 	coro->ctx = ctx;
	
// 	void* rsp = (uintptr_t)(buffer + size) & ~0xF;
// 	coro->_rsp = tina_init_stack(tina_wrap);
	
// 	// Allow tina_wrap() to finish initializing the stack.
// 	tina_swap(coro, body);
// }
