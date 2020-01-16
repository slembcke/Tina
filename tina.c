#include "stdbool.h"
#include "stdlib.h"
#include "tina.h"

// Defined in assembly.
void* tina_init_stack(void* sp);
uintptr_t tina_swap(tina* coro, uintptr_t value, void** sp);

// Function called when a coroutine body function exits.
void tina_finish(tina* coro, uintptr_t value){
	tina_yield(coro, value);
	
	// Any attempt to resume the coroutine after it's dead should call the error func.
	while(true){
		coro->_err("Tina error: Attempted to resume a dead coroutine.");
		tina_yield(coro, 0);
	}
}

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx, tina_err_func* err){
	tina* coro = buffer;
	(*coro) = (tina){
		.ctx = ctx,
		._err = err,
		._sp = tina_init_stack(buffer + size)
	};
	
	// Allow tina_wrap() to finish initializing the stack.
	tina_yield(coro, (uintptr_t)body);
	return coro;
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	return tina_swap(coro, value, &coro->_sp);
}
