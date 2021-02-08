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
	
	// Create a dummy coroutine for the main thread so we have a target to swith from/to using the symmetric API.
	// This doesn't allocate anything, Tina just needs somewhere to store the thread's context.
	tina dummy_coro;
	coro_main = tina_init_dummy(&dummy_coro);
	
	// Create the other coroutines.
	coro_a = tina_init(NULL, 64*1024, body_a, NULL);
	coro_b = tina_init(NULL, 64*1024, body_b, NULL);
	
	// Swap from the dummy coroutine to coroutine A.
	tina_swap(coro_main, coro_a, 0);
	
	printf("Back on the main coroutine and exiting.\n");
	return EXIT_SUCCESS;
}