#include "stdbool.h"
#include "stdlib.h"
#include "tina.h"

// Defined in assembly.
tina* tina_init_stack(tina* coro, void** sp_loc, void* sp, tina_func* body);
uintptr_t tina_swap(tina* coro, uintptr_t value, void** sp);

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx, tina_err_func* err){
	tina* coro = buffer;
	(*coro) = (tina){.ctx = ctx, ._err = err};
	return tina_init_stack(coro, &coro->_sp, buffer + size, body);
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	return tina_swap(coro, value, &coro->_sp);
}

// Function called when a coroutine body function exits.
void tina_finish(tina* coro, uintptr_t value){
	tina_yield(coro, value);
	
	// Any attempt to resume the coroutine after it's dead should call the error func.
	while(true){
		coro->_err("Tina error: Attempted to resume a dead coroutine.");
		tina_yield(coro, 0);
	}
}
