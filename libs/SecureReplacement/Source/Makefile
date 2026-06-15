# Compiler and Flags
CC = gcc
VERSION = 1.0.2
# Using underscore to prevent shell-to-compiler space issues
RELEASE_TS = $(shell date +"%Y-%m-%d_%H:%M:%S")

# Hardening Flags
CFLAGS = -Wall -Wextra -Werror -O2 -D_GNU_SOURCE \
         -fstack-protector-strong -D_FORTIFY_SOURCE=2 \
         -fPIC -fPIE \
         -DLIBSAFEC_VER='"$(VERSION)"' \
         -DRELEASE_TS='"$(RELEASE_TS)"'

# RELRO and NX Flags
LDFLAGS = -Wl,-z,relro,-z,now -Wl,-z,noexecstack
AR = ar
ARFLAGS = rcs

# Target Paths
LATEST_DIR = ../latest
LIB_STATIC = libsafec.a
LIB_SHARED = libsafec.so
DETAILS_FILE = $(LATEST_DIR)/LIBSAFECDetails.txt

# Files
SRCS = secure_replacement.c
OBJS = $(SRCS:.c=.o)
DEPS = secure_replacement.h

# Default: Build normally (with symbols)
all: $(LIB_STATIC) $(LIB_SHARED)

$(LIB_STATIC): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(LIB_SHARED): $(OBJS)
	$(CC) -shared $(LDFLAGS) -o $(LIB_SHARED) $(OBJS)

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# RELEASE Target: Strips symbols and moves to 'latest'
release: all
	@echo "Preparing Hardened Release v$(VERSION)..."
	@mkdir -p $(LATEST_DIR)
	
	@# Copy to latest
	@cp $(LIB_STATIC) $(LIB_SHARED) $(DEPS) $(LATEST_DIR)/
	
	@# Strip symbols from the 'latest' copies only
	@strip --strip-all $(LATEST_DIR)/$(LIB_SHARED)
	@strip --strip-debug $(LATEST_DIR)/$(LIB_STATIC)
	
	@# Generate Metadata and Checksums
	@echo "------------------------------------------------" > $(DETAILS_FILE)
	@echo "LIBSAFEC SECURITY & BUILD DETAILS" >> $(DETAILS_FILE)
	@echo "------------------------------------------------" >> $(DETAILS_FILE)
	@echo "Version Number    : $(VERSION)" >> $(DETAILS_FILE)
	@echo "Build Date/Time   : $(RELEASE_TS)" >> $(DETAILS_FILE)
	@echo "Hardening Status  : Canary, NX, Full RELRO, STRIPPED" >> $(DETAILS_FILE)
	@echo "------------------------------------------------" >> $(DETAILS_FILE)
	@echo "SHA256 FINGERPRINTS (INTEGRITY CHECK):" >> $(DETAILS_FILE)
	@sha256sum $(LATEST_DIR)/$(LIB_STATIC) >> $(DETAILS_FILE)
	@sha256sum $(LATEST_DIR)/$(LIB_SHARED) >> $(DETAILS_FILE)
	@echo "------------------------------------------------" >> $(DETAILS_FILE)
	@echo "Release Artifacts secured in $(LATEST_DIR)"

clean:
	rm -f *.o $(LIB_STATIC) $(LIB_SHARED)

.PHONY: all clean release