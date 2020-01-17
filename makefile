CFLAGS = -g -O2

.phony: clean default run

run: a.out
	./a.out

debug: a.out
	gdb a.out

clean:
	-rm *.o a.out

a.out: main.o tina.o
	cc $^
