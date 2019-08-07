#include <stdbool.h>
#include <inttypes.h>
#include <stdalign.h>

#include <stdlib.h>
#include <stdio.h>

typedef struct tina tina;
typedef uintptr_t tina_func(tina* coro, uintptr_t value);
typedef void tina_err_func(void);

tina_err_func* tina_err;

struct tina {
	void* ctx;
	
	void* _rsp;
	alignas(16) uint8_t _stack[];
};

uintptr_t tina_wrap(tina* coro, uintptr_t value);
uintptr_t tina_resume(tina* coro, uintptr_t value);
uintptr_t tina_yield(tina* coro, uintptr_t value);

void err(const char* message){
	fprintf(stderr, "Tina err: %s\n", message);
}

tina* tina_new(size_t size, tina_func* body, void* ctx){
	tina* coro = calloc(1, sizeof(tina) + size);
	coro->ctx = ctx;
	
	uintptr_t* rsp = (uintptr_t*)(coro->_stack + size);
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
	
	tina* coro = tina_new(16*1024, coro_body, NULL);
	while(tina_resume(coro, 0)){}
	printf("Success!\n");
	
	printf("Resuming again will crash...\n");
	tina_resume(coro, 0);
	
	return EXIT_SUCCESS;
}
