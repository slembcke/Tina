#pragma once
#include <stdbool.h>
#include <stdint.h>

// Coroutine type.
typedef struct tina tina;

// Coroutine body function type.
typedef uintptr_t tina_func(tina* coro, uintptr_t value);

// Error callback function type.
typedef void tina_error_handler(tina* coro, const char* message);

struct tina {
	// User defined context pointer.
	void* user_data;
	// User defined name. (optional)
	const char* name;
	// User defined error handler. (optional)
	tina_error_handler* error_handler;
	// Is the coroutine still running. (read only)
	bool running;
	
	// Private implementation details.
	void* _sp;
};

// Initialize a coroutine and return a pointer to it.
tina* tina_init(void* buffer, size_t size, tina_func* body, void* user_data);

// Yield execution to a coroutine.
uintptr_t tina_yield(tina* coro, uintptr_t value);
