.phony: clean default run

run: a.out
	./a.out

clean:
	-rm *.o a.out

%.o: %.asm
	nasm -fmacho64 $^

a.out: main.o tina.o
	cc $^