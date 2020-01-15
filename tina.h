#pragma once

#include <inttypes.h>

// Coroutine type.
typedef struct tina tina;

// Coroutine body function type.
typedef uintptr_t tina_func(tina* coro, uintptr_t value);

// Error callback function type.
typedef void tina_err_func(const char* message);

struct tina {
	// User defined context pointer.
	void* ctx;
	
	// Private implementation details.
	tina_err_func* _err;
	void* _sp;
};

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx, tina_err_func* err);

uintptr_t tina_yield(tina* coro, uintptr_t value);
