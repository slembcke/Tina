CFLAGS = -g -Os

.phony: clean default run

a.out: main.c tina.h
	cc -g $(CFLAGS) $< -o $@

a.exe: main.c tina.h
	x86_64-w64-mingw32-gcc -gstabs -O0 $< -o $@

clean:
	-rm a.*

main.o: tina.h

blob-init: win64-init.S
	x86_64-w64-mingw32-gcc -c $<
	objcopy -O binary win64-init.o win64-init.bin
	xxd -e -g8 win64-init.bin > win64-init.xxd