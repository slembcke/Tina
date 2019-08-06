.phony: clean default run

run: a.out
	./a.out

clean:
	-rm *.o a.out

coro.o: coro.asm
	nasm -fmacho64 $^

a.out: main.o coro.o
	cc $^