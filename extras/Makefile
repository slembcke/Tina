CFLAGS =  -O0 -g -Wall -Wextra -Werror -Wno-unused -I . -I ..
CFLAGS += -O3 -ffast-math

CXXFLAGS = $(CFLAGS) -Wno-missing-field-initializers

# Linux
LDFLAGS = -pthread -g
LDLIBS = -lm -lGL -lX11 -ldl

# wine-gcc
# CC = x86_64-w64-mingw32-gcc
# CXX = x86_64-w64-mingw32-g++
# CFLAGS += -Wno-cast-function-type -Wno-unknown-pragmas
# LDLIBS = -lgdi32

COMMON_OBJ = \
	common/libs/tinycthread.o \
	common/common.o \

TESTS = \
	test/jobs-throughput \

EXAMPLES = \
	examples/coro-simple \
	examples/jobs-mandelbrot \

default: $(TESTS) test/cpp-test $(EXAMPLES)

clean:
	-rm $(COMMON_OBJ) $(TESTS) test/cpp-test $(EXAMPLES) **/*.exe
	-rm win-asm/*.o win-asm/*.bin win-asm/*.xxd

$(EXAMPLES) $(TESTS): $(@:=.c) $(COMMON_OBJ)

test/cpp-test: test/cpp-test.cc common/libs/tinycthread.o
	$(CXX) $^ $(CFLAGS) $(LDFLAGS) -o $@

**/*.o: ../tina.h ../tina_jobs.h

win-asm: win-asm/win64-init.xxd win-asm/win64-swap.xxd

%.xxd: %.S
	x86_64-w64-mingw32-gcc -c $< -o $(<:.S=.o)
	objcopy -O binary $(<:.S=.o) $(<:.S=.bin)
	xxd -e -g8 $(<:.S=.bin) > $@

.PHONY: default clean win-asm
