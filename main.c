#include <stdbool.h>
#include <inttypes.h>

#include <stdlib.h>
#include <stdio.h>

typedef struct {
	
} tina;

tina* tina_new(void){return NULL;}
uintptr_t tina_resume(uintptr_t value){return value;}
uintptr_t tina_yield(uintptr_t value){return value;}

static uintptr_t coro_body(uintptr_t ctx){
	tina* coro = (tina*)ctx;
	
	for(unsigned i = 0; i < 10; i++){
		printf("coro: %u\n", i);
		tina_yield(true);
	}
	
	printf("coro_body return\n");
	return false;
}

int main(int argc, const char *argv[]){
	tina* coro = tina_new();
	while(tina_resume(0)){}
	
	printf("success\n");
	return EXIT_SUCCESS;
}
