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

tina* coro_main;
tina* coro_a;
tina* coro_b;

static uintptr_t body_a(tina* this_coro, uintptr_t value){
	printf("In coroutine A\n");
	
	// Swap to the next coroutine.
	tina_swap(this_coro, coro_b, 0);
	
	// When using symmetric coroutines you *must* not return from them.
	// It has nowhere to go and will cause a crash.
	// It's best to terminate them with a noreturn function like abort().
	abort();
}

static uintptr_t body_b(tina* this_coro, uintptr_t value){
	printf("In coroutine B\n");
	
	// Swap back to the main coroutine.
	tina_swap(this_coro, coro_main, 0);
	abort();
}

int main(void){
	printf("Starting on the main coroutine.\n");
	
	// Create an empty coroutine for the main thread so we have a target to switch from/to using the symmetric API.
	// This makes a coroutine without allocating a new stack for it, Tina just needs somewhere to store the thread's context.
	tina main_coro = TINA_EMPTY;
	coro_main = &main_coro;
	
	// Create the other coroutines.
	coro_a = tina_init(NULL, 64*1024, body_a, NULL);
	coro_b = tina_init(NULL, 64*1024, body_b, NULL);
	
	// Swap from the dummy coroutine to coroutine A.
	tina_swap(coro_main, coro_a, 0);
	
	printf("Back on the main coroutine and exiting.\n");
	return EXIT_SUCCESS;
}