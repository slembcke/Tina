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

uintptr_t tina_catch(tina* coro, uintptr_t value);

tina* tina_new(tina_func* body, void* ctx){
	tina* coro = calloc(1, sizeof(tina));
	coro->ctx = ctx;
	
	// Push tina_catch() and body() onto the stack.
	void** rsp = (void**)(coro->_stack + sizeof(coro->_stack));
	rsp[-2] = tina_catch;
	rsp[-3] = body;
	coro->_rsp = rsp - 3;
	
	return coro;
}

uintptr_t tina_yield(tina* coro, uintptr_t value){
	printf("tina_yield() NYI.");
	abort();
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
