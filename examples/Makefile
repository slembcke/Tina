CFLAGS =  -I.. -Wall -Werror -g -Os
LDFLAGS = -pthread -g

.phony: clean default run

all: mandelbrot

mandelbrot: mandelbrot.o
	$(CC) $(LDFLAGS) $< -o $@ 

*.o: ../tina.h ../tina_tasks.h

clean:
	-rm *.o mandelbrot
