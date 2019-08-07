#include <stdbool.h>
#include <inttypes.h>
#include <stdalign.h>

#include <stdlib.h>
#include <stdio.h>

typedef struct tina tina;
typedef uintptr_t tina_func(tina* coro, uintptr_t value);

struct tina {
	void* ctx;
	
	void* _rsp;
	alignas(16) uint8_t _stack[1024*1024];
};

uintptr_t tina_wrap(tina* coro, uintptr_t value);
uintptr_t tina_resume(tina* coro, uintptr_t value);
uintptr_t tina_yield(tina* coro, uintptr_t value);

tina* tina_new(tina_func* body, void* ctx){
	tina* coro = calloc(1, sizeof(tina));
	coro->ctx = ctx;
	
	// Setup the stack to call tina_wrap().
	uintptr_t* rsp = (uintptr_t*)(coro->_stack + sizeof(coro->_stack));
	*(--rsp) = (uintptr_t)tina_wrap;
	rsp -= 6;
	coro->_rsp = rsp;
	
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

void foobar(void);

int main(int argc, const char *argv[]){
	tina* coro = tina_new(coro_body, NULL);
	while(tina_resume(coro, 0)){}
	
	printf("success\n");
	return EXIT_SUCCESS;
}
