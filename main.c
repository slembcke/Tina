#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>

#include "tina.h"

tina* tina_init(void* buffer, size_t size, tina_func* body, void* ctx){
	tina* coro = buffer;
	coro->ctx = ctx;
	
	uintptr_t* rsp = (uintptr_t*)(buffer + size);
	// Push the wrapper function address onto the stack.
	*(--rsp) = (uintptr_t)tina_wrap;
	// Push zeros onto the stack for the saved registers.
	rsp -= 6;
	coro->_rsp = rsp;
	
	// Start the wrapper function.
	tina_resume(coro, (uintptr_t)body);
	
	return coro;
}

// ---------------------------

void err(const char* message){
	fprintf(stderr, "Tina err: %s\n", message);
}

static uintptr_t coro_body(tina* coro, uintptr_t value){
	printf("coro_body() enter\n");
	
	for(unsigned i = 0; i < 10; i++){
		printf("coro_body(): %u\n", i);
		tina_yield(coro, true);
	}
	
	printf("coro_body() return\n");
	return false;
}

int main(int argc, const char *argv[]){
	tina_err = err;
	
	uint8_t buffer[16*1024];
	tina* coro = tina_init(buffer, sizeof(buffer), coro_body, NULL);
	
	while(tina_resume(coro, 0)){}
	printf("Success!\n");
	
	printf("Resuming again will crash...\n");
	tina_resume(coro, 0);
	
	return EXIT_SUCCESS;
}
