/*
	Copyright (c) 2021 Scott Lembcke

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>

#include "tina.h"

static uintptr_t coro_body(tina* coro, uintptr_t value);
static void coro_error(tina* coro, const char* message);

int main(int argc, const char *argv[]){
	// Initialize a coroutine with some stack space, a body function, and some user data.
	tina* coro = tina_init(NULL, 1024*1024, coro_body, "A user data pointer.");

	// Optionally set some debugging values.
	coro->name = "MyCoro";
	
	// Call tina_resume() to call into the other coroutine.
	// You can optionally pass a value through to the coroutine as well.
	while(!coro->completed) tina_resume(coro, 0);
	
	printf("Resuming again is an error and should throw an assertion (or abort() if assertions are disabled).\n");
	printf("Wait for it...\n");
	tina_resume(coro, 0);
	
	free(coro->buffer);
	return EXIT_SUCCESS;
}

// The body function is pretty straightforward.
// It get's passed the coroutine and the first value passed to tina_resume().
static uintptr_t coro_body(tina* coro, uintptr_t value){
	printf("coro_body() enter\n");
	printf("user_data: '%s'\n", (char*)coro->user_data);
	
	for(unsigned i = 0; i < 3; i++){
		printf("coro_body(): %u\n", i);
		// Yielding suspends this coroutine and returns control back to the caller.
		tina_yield(coro, 0);
	}
	
	// The return value is returned from tina_resume() in the caller.
	return 0;
}
