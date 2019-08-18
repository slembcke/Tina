#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>

#include "tina.h"

static void handle_tina_err(const char* err){
	puts(err);
	abort();
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
	tina_err = handle_tina_err;

	size_t size = 1024*1024 - 1;
	void* buffer = malloc(size);
	tina* coro = tina_init(buffer, size, coro_body, NULL);
	
	while(tina_resume(coro, 0)){}
	printf("Success!\n");
	
	printf("Resuming again will crash...\n");
	tina_resume(coro, 0);
	
	return EXIT_SUCCESS;
}
