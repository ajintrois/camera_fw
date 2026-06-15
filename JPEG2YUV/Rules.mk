###############################################################################
#
# Copyright (c) 2016-2023, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###############################################################################
#********************************************************************************************/
#*		Project	   	:	Jetson ORIN Camera/NVR			       */
#*		Author/Modified By 	:	Maheen Rasheed				       */
#*		Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.	       */
#********************************************************************************************/

# Clear the flags from env
CPPFLAGS :=
LDFLAGS :=

# Verbose flag
ifeq ($(VERBOSE), 1)
AT =
else
AT = @
endif

# ARM ABI of the target platform
ifeq ($(TEGRA_ARMABI),)
TEGRA_ARMABI ?= aarch64-linux-gnu
endif

# Location of the target rootfs
ifeq ($(shell uname -m), aarch64)
TARGET_ROOTFS :=
else
ifeq ($(TARGET_ROOTFS),)
$(error Please specify the target rootfs path if you are cross-compiling)
endif
endif

# Location of the CUDA Toolkit
CUDA_PATH 	:= /usr/local/cuda

# Use absolute path for better access from everywhere
#TOP_DIR 	:= $(shell pwd | awk '{split($$0, f, "/samples"); print f[1]}')
TOP_DIR 	:= /usr/src/jetson_multimedia_api
CLASS_DIR 	:= $(TOP_DIR)/samples/common/classes
ALGO_CUDA_DIR 	:= $(TOP_DIR)/samples/common/algorithm/cuda
ALGO_TRT_DIR 	:= $(TOP_DIR)/samples/common/algorithm/trt

# Ashutosh : Add OPENCV For OSD
# 1. Check if the system has exactly version 4.8.0
# --atleast-version or --exact-version can be used here
SYS_OPENCV_VER := $(shell pkg-config --modversion opencv4 2>/dev/null)

ifeq ($(SYS_OPENCV_VER), 4.8.0)
    # SYSTEM MATCH: Use system-wide OpenCV 4.8.0
    OPENCV_CPPFLAGS := $(shell pkg-config --cflags opencv4)
    OPENCV_LDFLAGS  := $(shell pkg-config --libs opencv4)
	OPENCV_MSG      := ">>> Using System-wide OpenCV 4.8.0"
else
    # FALLBACK: Use local development folder
    OPENCV_INSTALL_DIR := /home/mtx003/MtxCameraApps/Libraries/opencv-4.8.0/build/install
    OPENCV_CPPFLAGS := -isystem "$(OPENCV_INSTALL_DIR)/include/opencv4"
    OPENCV_LDFLAGS  := -L"$(OPENCV_INSTALL_DIR)/lib" \
                       -lopencv_core -lopencv_imgproc -lopencv_imgcodecs -lopencv_highgui
	OPENCV_MSG      := ">>> System OpenCV is $(SYS_OPENCV_VER). Falling back to local 4.8.0"
endif

# OPENCV_INSTALL_DIR := /home/mtx003/MtxCameraApps/Libraries/opencv-4.8.0/build/install
# OPENCV_INCLUDES_MODULES := /home/mtx003/MtxCameraApps/Libraries/opencv-4.8.0/build
# OPENCV_LIB_DIR := $(OPENCV_INSTALL_DIR)/lib

ifeq ($(shell uname -m), aarch64)
CROSS_COMPILE :=
else
CROSS_COMPILE ?= aarch64-unknown-linux-gnu-
endif
AS             = $(AT) $(CROSS_COMPILE)as
LD             = $(AT) $(CROSS_COMPILE)ld
CC             = $(AT) $(CROSS_COMPILE)gcc
CPP            = $(AT) $(CROSS_COMPILE)g++
AR             = $(AT) $(CROSS_COMPILE)ar
NM             = $(AT) $(CROSS_COMPILE)nm
STRIP          = $(AT) $(CROSS_COMPILE)strip
OBJCOPY        = $(AT) $(CROSS_COMPILE)objcopy
OBJDUMP        = $(AT) $(CROSS_COMPILE)objdump
NVCC           = $(AT) $(CUDA_PATH)/bin/nvcc -ccbin $(filter-out $(AT), $(CPP))

# Specify the logical root directory for headers and libraries.
ifneq ($(TARGET_ROOTFS),)
CPPFLAGS += --sysroot=$(TARGET_ROOTFS)
LDFLAGS += \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/lib/$(TEGRA_ARMABI) \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI) \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)/nvidia \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)/tegra \
	-Wl,-rpath-link=$(TARGET_ROOTFS)/$(CUDA_PATH)/lib64
endif

# All common header files
CPPFLAGS += -std=c++11 \
	-O2 -D_FORTIFY_SOURCE=2 \
	-I"$(TOP_DIR)/include" \
	-I"$(TOP_DIR)/include/libjpeg-8b" \
	-I"$(ALGO_CUDA_DIR)" \
	-I"$(ALGO_TRT_DIR)" \
	-I"$(TARGET_ROOTFS)/$(CUDA_PATH)/include" \
	-I"$(TARGET_ROOTFS)/usr/include/$(TEGRA_ARMABI)" \
	-I"$(TARGET_ROOTFS)/usr/include/libdrm" \
	-isystem "$(OPENCV_INSTALL_DIR)/include/opencv4"

CPPFLAGS += $(OPENCV_CPPFLAGS)
LDFLAGS  += $(OPENCV_LDFLAGS)

#-I"$(TARGET_ROOTFS)/usr/include/opencv4"

# LDFLAGS += -Wl,-rpath,'$$ORIGIN/lib'
#    -Wl,-rpath,$(OPENCV_INSTALL_DIR)/lib \
#	-Wl,--disable-new-dtags \

# All common dependent libraries
LDFLAGS += \
    -lpthread -lnvv4l2 -lEGL -lGLESv2 -lX11 \
    -lnvbufsurface -lnvbufsurftransform -lnvjpeg -lnvosd -ldrm \
    -lcuda -lcudart -lvulkan \
    -L"$(TARGET_ROOTFS)/$(CUDA_PATH)/lib64" \
    -L"$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)" \
    -L"$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)/nvidia" \
    -L"$(TARGET_ROOTFS)/usr/lib/$(TEGRA_ARMABI)/tegra"
