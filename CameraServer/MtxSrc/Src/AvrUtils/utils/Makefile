all: 
	gcc GetTime.c ../CircFifo.c -o GetTime -D_FORTIFY_SOURCE=2 -O2 -Wl,-rpath,
	strip --strip-unneeded GetTime
	objcopy --strip-unneeded GetTime
	@cp GetTime ../../../flash/.
