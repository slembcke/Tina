blobs: blob-init blob-swap

blob-%: win64-%.S
	x86_64-w64-mingw32-gcc -c $<
	objcopy -O binary $(<:.S=.o) $(<:.S=.bin)
	xxd -e -g8 $(<:.S=.bin) > $(<:.S=.xxd)

clean:
	-rm *.o *.bin *.xxd
