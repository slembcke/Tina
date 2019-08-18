CFLAGS = -g -O0

.phony: clean default run

run: a.out
	./a.out

debug: a.out
	gdb a.out

clean:
	-rm *.o a.out

%.o: %.asm
	nasm -g -felf64 $^

a.out: main.o tina.o
	cc $^