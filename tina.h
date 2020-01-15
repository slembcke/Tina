#pragma once

#include <inttypes.h>

typedef struct tina tina;

// Coroutine body function type.
typedef uintptr_t tina_func(tina* coro, uintptr_t value);

// Error callback function type.
typedef void tina_err_func(const char* message);

// Coroutine struct.
struct tina {
	// User defined context pointer.
	void* ctx;
	
	// Private implementation details.
	void* _rsp;
	tina_err_func* _err;
	uint8_t _stack[];
};

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx, tina_err_func* err);

uintptr_t tina_swap(tina* coro, uintptr_t value);
