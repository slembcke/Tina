CFLAGS =  -I.. -Wall -Werror -g -Os
LDFLAGS = -pthread -lm -lGL -lX11 -ldl -g

.phony: clean default run

all: mandelbrot

mandelbrot: mandelbrot.o
	$(CC) $< -o $@ $(LDFLAGS)

*.o: ../tina.h ../tina_tasks.h

clean:
	-rm *.o mandelbrot
