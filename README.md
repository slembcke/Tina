# Tina
Tina is a teeny tiny, header only, coroutine library!

Features:
* Cross platform.
* Super fast assembly language implementation.
* Super simple API. Basically just create coroutines and swap them.

```C
#include <stdlib.h>
#include <stdio.h>

#define TINA_IMPLEMENTATION
#include "tina.h"

static uintptr_t coro_body(tina* coro, uintptr_t value);
static void coro_error(tina* coro, const char* message);

int main(int argc, const char *argv[]){
	// Initialize a coroutine with some stack space, a body function, and some user data.
	uint8_t buffer[1024*1024];
	void* user_data = "some user data";
	tina* coro = tina_init(buffer, sizeof(buffer), coro_body, user_data);
	
	// Optionally set some debugging values.
	coro->name = "MyCoro";
	coro->error_handler = coro_error;
	
	// Call tina_yield() to switch coroutines.
	// You can optionally pass a value through to the coroutine as well.
	while(coro->running) tina_yield(coro, 0);
	
	printf("Resuming again will call coro_error()\n");
	tina_yield(coro, 0);
	
	return EXIT_SUCCESS;
}

// The body function is pretty straightforward.
// It get's passed the coroutine and the first value passed to tina_yield().
static uintptr_t coro_body(tina* coro, uintptr_t value){
	printf("coro_body() enter\n");
	printf("user_data: '%s'\n", (char*)coro->user_data);
	
	for(unsigned i = 0; i < 3; i++){
		printf("coro_body(): %u\n", i);
		tina_yield(coro, 0);
	}
	
	// The return value is returned from tina_yield() in the caller.
	return 0;
}

static void coro_error(tina* coro, const char* message){
	printf("Tina error (%s): %s\n", coro->name, message);
}
```

So far it's been mildly tested to run on:
* 64 bit Intel for Mac/Linux/Windows using GCC/Clang
* 32 bit armv for Raspbian using GCC

Adding support for new platforms should be pretty straightforward and just require a couple dozen lines of assembly code.

TODO:
* Need to do more Windows testing.
* MSVC support?
* Arm 64 implementation. (Android/iOS I guess?)
* Look into WASM see if that is possible.
