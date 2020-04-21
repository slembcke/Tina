CFLAGS =  -O0 -g -Wall -Werror -Wno-unused
LDFLAGS = -pthread -lm -lGL -lX11 -ldl -g

# CFLAGS += -Os -ffast-math

.phony: clean default run

all: mandelbrot

mandelbrot: mandelbrot.o
	$(CC) $< -o $@ $(LDFLAGS)

*.o: ../tina.h ../tina_tasks.h

clean:
	-rm *.o mandelbrot
