#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>

#include "tina.h"

static void handle_tina_err(const char* err){
	puts(err);
	// abort();
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
	uint8_t buffer[1024*1024];
	tina* coro = tina_init(buffer, sizeof(buffer), coro_body, NULL, handle_tina_err);
	
	while(tina_yield(coro, 0)){}
	printf("Success!\n");
	
	printf("Resuming again will call handle_tina_err()\n");
	tina_yield(coro, 0);
	tina_yield(coro, 0);
	
	return EXIT_SUCCESS;
}
