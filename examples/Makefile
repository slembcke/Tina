CFLAGS =  -O0 -g -Wall -Werror -Wno-unused
CFLAGS += -O3 -ffast-math

# Linux
LDFLAGS = -pthread -g
LDLIBS = -lm -lGL -lX11 -ldl

# Windows
# LDLIBS = -lgdi32

EXAMPLES = \
	coro-simple \
	jobs-mandelbrot \

all: $(EXAMPLES)

clean:
	-rm *.o $(EXAMPLES)

$(EXAMPLES): $(@:=.c) libs/tinycthread.o

*.o: ../tina.h ../tina_jobs.h
