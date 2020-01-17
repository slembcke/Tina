#include "stdbool.h"
#include "stdlib.h"
#include "tina.h"

// Defined in assembly.
tina* tina_context(tina* coro, void** sp_loc, void* sp, tina_func* body);
uintptr_t tina_swap(tina* coro, uintptr_t value, void** sp);

tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data){
	tina* coro = buffer;
	(*coro) = (tina){.user_data = user_data, .running = true};
	return tina_context(coro, &coro->_sp, buffer + size, body);
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	return tina_swap(coro, value, &coro->_sp);
}

// Function called when a coroutine body function exits.
void tina_finish(tina* coro, uintptr_t value){
	coro->running = false;
	tina_yield(coro, value);
	
	// Any attempt to resume the coroutine after it's dead should call the error func.
	while(true){
		if(coro->error_handler) coro->error_handler(coro, "Attempted to resume a dead coroutine.");
		tina_yield(coro, 0);
	}
}
