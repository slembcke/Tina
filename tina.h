#pragma once

#include <inttypes.h>

// Error callback function type.
typedef void tina_err_func(const char* message);
// Error message function. Defaults to puts().
extern tina_err_func* tina_err;

// Coroutine struct.
typedef struct {
	// User defined context pointer.
	void* ctx;
	
	// Private implementation details.
	void* _rsp;
	uint8_t _stack[];
} tina;

// Coroutine body function type.
typedef uintptr_t tina_func(tina* coro, uintptr_t value);

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx);

uintptr_t tina_swap(tina* coro, uintptr_t value);
