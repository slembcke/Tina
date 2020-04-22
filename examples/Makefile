CFLAGS =  -O0 -g -Wall -Werror -Wno-unused
CFLAGS += -Os -ffast-math

# Linux
LDFLAGS = -pthread -lm -lGL -lX11 -ldl -g

# Windows
# CC = x86_64-w64-mingw32-gcc -Wno-unknown-pragmas
# LDFLAGS = -lgdi32

.phony: clean default run

all: mandelbrot

mandelbrot: mandelbrot.o tinycthread.o
	$(CC) $^ -o $@ $(LDFLAGS)

*.o: ../tina.h ../tina_tasks.h

clean:
	-rm *.o mandelbrot mandelbrot.exe
