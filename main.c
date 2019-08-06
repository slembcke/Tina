#include <stdlib.h>
#include <stdio.h>

unsigned coro_resume(unsigned);

int main(int argc, const char *argv[]){
	unsigned value = coro_resume(5);
	printf("value: %u\n", value);
	
	return EXIT_SUCCESS;
}
