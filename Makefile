CFLAGS =  -O0 -g -Wall -Wextra -Werror -Wno-unused -I.
CFLAGS += -O3 -ffast-math

CXXFLAGS = $(CFLAGS) -Wno-missing-field-initializers

# Linux
LDFLAGS = -pthread -g
LDLIBS = -lm -lGL -lX11 -ldl

# Windows
# LDLIBS = -lgdi32

COMMON_OBJ = \
	common/libs/tinycthread.o \
	common/common.o \

TESTS = \
	test/jobs-throughput \

EXAMPLES = \
	examples/coro-simple \
	examples/jobs-mandelbrot \

.PHONY: all clean %.gdb

all: $(TESTS) test/cpp-test $(EXAMPLES)

clean:
	-rm **/*.o $(TESTS) test/cpp-test $(EXAMPLES)

%.gdb: %
	./gdb.sh $^

$(EXAMPLES) $(TESTS): $(@:=.c) $(COMMON_OBJ)

**/*.o: tina.h tina_jobs.h
