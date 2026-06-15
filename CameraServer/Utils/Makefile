all: getver cfgenc

getver : getversionV3.c 
	@echo generating getversion
	@gcc -o getversionV3 getversionV3.c -D_FORTIFY_SOURCE=2 -O2 -Wl,-rpath,$(ORIGIN) -lssl -lcrypto 
	@strip --strip-unneeded getversionV3
	@objcopy --strip-unneeded getversionV3

cfgenc : ConfigEncProcess.c
	@echo generating cfgenc
	@gcc -o ConfigEnckeyset ConfigEncProcess.c -D_FORTIFY_SOURCE=2 -O2 -Wl,-rpath,$(ORIGIN) 
	@strip --strip-unneeded ConfigEnckeyset
	@objcopy --strip-unneeded ConfigEnckeyset

clean:
	@echo cleaning utils..
	@if [ -f "getversionV3" ]; then rm getversionV3; fi
	@if [ -f "ConfigEnckeyset" ]; then rm ConfigEnckeyset; fi

