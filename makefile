CFLAGS = -g -Os

.phony: clean default run

a.out: main.c tina.h
	cc -g $(CFLAGS) $< -o $@

a.exe: main.c tina.h
	x86_64-w64-mingw32-gcc -gstabs -O0 $< -o $@

clean:
	-rm a.*

main.o: tina.h
