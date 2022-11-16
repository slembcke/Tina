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

//#include <unistd.h> // for getstacksize()
//#include <sys/mman.h> // for mmap()

#include "tina.h"

static void* coro_body(tina* coro, void* value);

int main(int argc, const char *argv[]){
	size_t buffer_size = 256*1024;
	void* buffer = NULL; // Tina will allocate a buffer for you if you pass NULL
	
	/*
	// Using guard pages is not hard. For example on Unix-like OSes you would do the following:
	// Allocate some memory using mmap(). (Note: MAP_STACK is required by OpenBSD)
	int map_flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK;
	buffer = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, map_flags, -1, 0);
	// Save the first page for the tina header.
	// Mark the second page as protected to keep the stack from growing into the header.
	mprotect(buffer + getpagesize(), getpagesize(), PROT_NONE);
	// Shrink the buffer size by a page at the end and mark that as protected also.
	buffer_size -= getpagesize();
	mprotect((uint8_t*)buffer + buffer_size, getpagesize(), PROT_NONE);
	*/

	// Initialize a coroutine with some stack space, a body function, and some user data.
	void* user_data = "An optional user data pointer";
	tina* coro = tina_init(buffer, buffer_size, coro_body, user_data);
	
	// You can also name your coroutines for debugging purposes.
	coro->name = "MyCoro";
	
	// Call tina_resume() to switch to the coroutine.
	// The body function will now start.
	// The value you send is passed as the 'value' arg to the body function.
	void* value = tina_resume(coro, "hello");
	
	// Now each time you call tina_resume() afterwards it will continue executing from the most recent tina_yield().
	// The value returned by tina_resume() will be the value passed to the matching tina_yield(), and vice-versa.
	// When the body function returns, that value will also be returned from tina_resume() and 'tina.complete' will become true.
	while(!coro->completed) tina_resume(coro, NULL);
	
	// The coroutine body function has returned. So attempting to resume it again will fail.
	printf("The coroutine has finished. Calling it again will crash, like this!\n");
	tina_resume(coro, 0);
	
	free(coro->buffer);
	return EXIT_SUCCESS;
}

// The body function is pretty straightforward.
// It get's passed the coroutine and the first value passed to tina_resume().
static void* coro_body(tina* coro, void* value){
	printf("coro_body() enter\n");
	printf("user_data: '%s'\n", (char*)coro->user_data);
	
	for(unsigned i = 0; i < 3; i++){
		printf("coro_body(): %u\n", i);
		// Yielding suspends this coroutine and returns control back to the caller.
		tina_yield(coro, 0);
	}
	
	printf("coro_body() return\n");
	// The return value is returned from tina_resume() in the caller.
	return 0;
}
