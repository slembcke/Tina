CFLAGS =  -O0 -g -Wall -Werror -Wno-unused
CFLAGS += -O3 -ffast-math

# Linux
LDFLAGS = -pthread -lm -lGL -lX11 -ldl -g

# Windows
# LDFLAGS = -lgdi32

.phony: clean default run

all: mandelbrot

mandelbrot: mandelbrot.o tinycthread.o
	$(CC) $^ -o $@ $(LDFLAGS)

*.o: ../tina.h ../tina_jobs.h

clean:
	-rm *.o mandelbrot mandelbrot.exe
