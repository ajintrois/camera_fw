# VERSION NO. for Nano Camera IMX264(5MP). F224_xxx.
#VERSION4KRLVD = F248_280

#V2


AESKEY="test"
AESIV="key"

.PHONY: util mtx 


all: util mtx 

mtx:	
	sync
	@make -C Src	

util:	
	sync
	make -C ../Utils	


codecheck:
	@echo Checking codes using Codechecker on $(shell pwd)..
	@./CodeChk.sh	
	
clean:
	@echo cleaning mtx output
	@if [ -f "$(wildcard *.bin)" ]; then rm $(wildcard *.bin) $(wildcard *.hash); else echo cleaned; fi
	@make -C Src clean
	@make -C ../Utils clean

