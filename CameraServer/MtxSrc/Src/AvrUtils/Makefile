CSOURCES=$(wildcard *.c)
COBJECTS=$(CSOURCES:%.c=objs/%.o)

.PHONY: app utils

all: clean utils $(COBJECTS) 

$(COBJECTS): objs/%.o:%.c
	@echo compiling $@ from $<
	g++ -c -o $@ $< -D_FORTIFY_SOURCE=2 -O2 -Wl,-rpath
	
utils:
	@make -C utils

clean: 
	@echo cleaning project	
	@if [ -f objs/CircFifo.o ]; then rm $(wildcard objs/*.o); else echo cleaned; fi
#all: 
#	rm -f objs/*.o
#	cc -c Maheen.c -o objs/Maheen.o
#	cc -c CircFifo.c -o objs/CircFifo.o
#	cc -c Timers.c -o objs/Timers.o
#	cc -c UART2.c -o objs/UART2.o
#	cc -c I2C.c -o objs/I2C.o
#	cp objs/*.o ../objs/	
