CFLAGS = -g -O3

.phony: clean default run

a.out: task.c
	cc $(CFLAGS) $< -o $@ -pthread

a.exe: main.c # win64-init.S win64-swap.S
	x86_64-w64-mingw32-gcc -gstabs -O0 $^ -o $@

a.out a.exe: tina.h tina_task.h

clean:
	-rm a.*

main.o: tina.h

blob-%: win64-%.S
	x86_64-w64-mingw32-gcc -c $<
	objcopy -O binary $(<:.S=.o) $(<:.S=.bin)
	xxd -e -g8 $(<:.S=.bin) > $(<:.S=.xxd)

blobs: blob-init blob-swap
