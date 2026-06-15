include Rules.mk

CSOURCES=$(wildcard *.c)
COBJECTS=$(CSOURCES:%.c=objs/%.o)
CPPSOURCES=$(wildcard *.cpp)
CPPOBJECTS=$(CPPSOURCES:%.cpp=objs/%.o)
NVIDIASRCS=$(wildcard $(CLASS_DIR)/*.cpp)
NVIDIAOBJS=$(NVIDIASRCS:%.cpp=%.o)
MEDIASERVEROBJS=$(wildcard ../MediaServer/MtxStreamer/objs/*.o)

WARNING_FLAGS="-Wall -Wextra -pedantic"
C_FLAGS="\$(WARNING_FLAGS) -O2 -Wl,-rpath,\$(ORIGIN)"

.PHONY: Capture5M

ALL_CPPFLAGS := $(addprefix -Xcompiler ,$(filter-out -std=c++11, $(CPPFLAGS)))

# CUDA code generation flags
ENCODE_SM53 := -gencode arch=compute_53,code=sm_53
GENCODE_SM62 := -gencode arch=compute_62,code=sm_62
GENCODE_SM72 := -gencode arch=compute_72,code=sm_72
GENCODE_SM87 := -gencode arch=compute_87,code=sm_87
GENCODE_SM_PTX := -gencode arch=compute_72,code=compute_72
GENCODE_FLAGS := $(GENCODE_SM53) $(GENCODE_SM62) $(GENCODE_SM72) $(GENCODE_SM87) $(GENCODE_SM_PTX)

all: Capture5M

$(CLASS_DIR)/%.o: $(CLASS_DIR)/%.cpp
	$(AT)$(MAKE) -C $(CLASS_DIR)

$(COBJECTS): objs/%.o:%.c
	@echo compiling $@ from $<
	$(CPP) -c -o $@ $< $(CPPFLAGS) $(INCLUDE) -D_FORTIFY_SOURCE=2

$(CPPOBJECTS): objs/%.o:%.cpp
	@echo compiling $@ from $<
	$(CPP) $(CPPFLAGS) -c -o $@ $< -D_FORTIFY_SOURCE=2

deBayer5M.o: deBayer5M.cu
	@echo "Compiling: $<"
	$(NVCC) $(ALL_CPPFLAGS) $(GENCODE_FLAGS) -c $< -D_FORTIFY_SOURCE=2

Capture5M : $(COBJECTS) $(CPPOBJECTS) $(NVIDIAOBJS) $(MEDIASERVEROBJS) deBayer5M.o
	@echo generating Capture5M
	@echo linking $(COBJECTS) $(CPPOBJECTS) $(NVIDIAOBJS) $(MEDIASERVEROBJS) deBayer5M.o -o $@ $(CPPFLAGS) $(LDFLAGS) $(INCLUDE) 
	$(CPP) $(COBJECTS) $(CPPOBJECTS) $(NVIDIAOBJS) $(MEDIASERVEROBJS) deBayer5M.o -o $@ $(CPPFLAGS) $(LDFLAGS) $(INCLUDE) -lrt -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
	strip --strip-unneeded $@
	objcopy --strip-unneeded $@
	cp $@ ../../flash/$@

clean: 
	@echo cleaning project	
	@if [ -f "objs/Capture5M.o" ]; then rm objs/*.o; fi
	@if [ -f "Capture5M" ]; then rm Capture5M; fi


