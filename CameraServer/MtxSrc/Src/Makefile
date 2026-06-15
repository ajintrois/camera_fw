DPKG_EXPORT_BUILDFLAGS=1
#include /usr/share/dpkg/buildflags.mk

CSOURCES=$(wildcard *.c)
COBJECTS=$(CSOURCES:%.c=objs/%.o)
CPPSOURCES=$(wildcard *.cpp)
CPPOBJECTS=$(CPPSOURCES:%.cpp=objs/%.o)
WARNING_FLAGS="-Wall -Wextra -pedantic"
C_FLAGS += $(WARNING_FLAGS) -O2 -Wl,-rpath,$(ORIGIN)
AVRUTILOBJS=$(wildcard AvrUtils/objs/*.o)

.PHONY: TroisStream Capture5M Capture8M

all: avrinterface media Capture5M Capture8M TroisStream 

$(COBJECTS): objs/%.o:%.c
	@echo compiling $@ from $<
	g++ -c -o $@ $<  -D_FORTIFY_SOURCE=2 

$(CPPOBJECTS): objs/%.o:%.cpp
	@echo compiling $@ from $<
	g++ -c -o $@ $< -D_FORTIFY_SOURCE=2 

TroisStream:$(COBJECTS) $(CPPOBJECTS) 
	@echo linking $(COBJECTS) $(CPPOBJECTS) $(AVRUTILOBJS) $@
	g++ -o $@ $(COBJECTS) $(CPPOBJECTS) $(AVRUTILOBJS) -lssl -lcrypto -lrt -lpthread -lgpiod
	strip --strip-unneeded $@
	objcopy --strip-unneeded $@
	cp $@ ../flash/$@

Capture8M:
	@make -C Capture8M

Capture5M:
	@make -C Capture5M

media:
	@make -C MediaServer/MtxStreamer

avrinterface:	
	@make -C AvrUtils

clean: 
	@echo cleaning project
	@if [ -f objs/circfifo.o ]; then rm $(wildcard objs/*.o); else echo cleaned objs; fi
	@if [ -f TroisStream ]; then rm TroisStream; else echo cleaned TroisStream; fi
	@make -C Capture8M clean
	@make -C Capture5M clean
	@make -C MediaServer/MtxStreamer clean
	


