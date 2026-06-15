#********************************************************************************************/
#*		Project	   	:	Jetson ORIN Camera/NVR			       */
#*		Author/Modified By 	:	Maheen Rasheed				       */
#*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
#********************************************************************************************/
include Rules.mk

CPPSOURCES=$(wildcard *.cpp)
CPPOBJECTS=$(CPPSOURCES:%.cpp=objs/%.o)



.PHONY: app utils


ALL_CPPFLAGS := $(addprefix -Xcompiler ,$(filter-out -std=c++11, $(CPPFLAGS)))

OBJS := $(SRCS:.cpp=.o)

all: clean $(CPPOBJECTS) 


$(CPPOBJECTS): objs/%.o:%.cpp
	@echo compiling $@ from $<
	$(CPP) $(CPPFLAGS) -c -o $@ $< -O2 -D_FORTIFY_SOURCE=2 -O2 -Wl,-rpath,

clean: 
	@echo cleaning project	
	@if [ -f objs/RTSPTest.o ]; then rm objs/RTSPTest.o; else echo cleaned; fi


