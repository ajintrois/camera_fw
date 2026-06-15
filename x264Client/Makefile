#********************************************************************************************/
#* Project     :   Jetson ORIN Camera/NVR                                           */
#* Author/Modified By  :   Maheen Rasheed, Ashutosh Kumar                           */
#* Mediatronix Pvt Ltd,Pappanamcode,Industrial Estate,Trivandrum.                   */
#********************************************************************************************/

# --- Paths & Configuration ---
VERSION_H   = version.h
VERSION_TXT = version.txt
EXE         = 
MTX_APPS    = x264Client$(EXE)

# x264 Master Integration
LIB_ROOT      = ../../MediaServer/libs/x264
X264_INC      = $(LIB_ROOT)/include
X264_LIB_DIR  = $(LIB_ROOT)/lib
X264_CONFIG_H = $(X264_INC)/x264_config.h
SETUP_SCRIPT  = ./scripts/setup_x264.sh
EXPECTED_X264 = "0.165.x"
X264_STATIC_LIB = $(X264_LIB_DIR)/libx264.a

# Ashutosh C Functions Replacement (libsafec)
LIBSAFEC_PATH = ../libs/SecureReplacement/latest
LIBSAFEC_INC  = $(LIBSAFEC_PATH)
LIBSAFEC_LIB  = $(LIBSAFEC_PATH)/libsafec.a

# Versioning logic files
ALL_FILES    = $(wildcard *.c *.cpp *.h *.py)
HASH_TARGETS = $(filter-out $(VERSION_H) $(VERSION_TXT) .source_hash .new_hash, $(ALL_FILES))

.PHONY: all clean build_revision build_dependencies check_version

# --- Library Presence Checks ---
CHECK_LIBSAFEC := $(shell if [ -f $(LIBSAFEC_LIB) ]; then echo "exists"; else echo "missing"; fi)
ifeq ($(CHECK_LIBSAFEC), missing)
  $(error [SECURITY ERROR] libsafec.a not found at $(LIBSAFEC_LIB). Build the library first!)
endif

# --- Main Build Sequence ---

# We remove 'clean' from 'all' to allow smart rebuilding, 
# but you can add it back if you prefer a fresh build every time.
all: build_dependencies check_version build_revision $(MTX_APPS)

# 1. Trigger the download/hardening script if x264 is missing - Shared Lib Method - Optional
build_dependencies_shared:
	@if [ ! -f $(X264_LIB_DIR)/libx264.so ]; then \
		echo "x264 library missing. Running hardened setup script..."; \
		bash $(SETUP_SCRIPT); \
	fi

# 1. Trigger the download/hardening script if x264 is missing - Static Lib Method - Recommended
build_dependencies:
	@if [ ! -f $(X264_STATIC_LIB) ]; then \
		echo "x264 library missing. Running hardened setup script..."; \
		bash $(SETUP_SCRIPT); \
	fi

# 2. Guard against old versions (NIST Compliance check)
check_version: build_dependencies
	@if [ ! -f $(X264_CONFIG_H) ]; then echo "Error: x264 config missing."; exit 1; fi
	@CURRENT_VER=$$(grep "X264_POINTVER" $(X264_CONFIG_H) | awk -F'"' '{print $$2}'); \
	case $$CURRENT_VER in \
		0.165.*) echo "x264 Version Match: $$CURRENT_VER" ;; \
		*) echo "ERROR: Detected old x264 version $$CURRENT_VER. Need $(EXPECTED_X264)"; exit 1 ;; \
	esac

# 3. Compile and Link for Shared Library Linking
$(MTX_APPS_SHARED): x264Client.o
	#gcc -o $@ x264Client.o -L$(X264_LIB_DIR) -lx264 $(LIBSAFEC_LIB) -lm -lpthread -ldl -lrt \
	-Wl,-rpath,'$$ORIGIN/$(LIB_ROOT)/lib' -s
	@echo "Copying x264Client >> $(MTX_APPS) to release folder"
	#@mkdir -p ../../MediaServer/release
	@cp $(MTX_APPS) ../../build
	@cp version.txt ../../build/$(MTX_APPS)-version.txt

# Update the linking command For Static Linking
$(MTX_APPS): x264Client.o
	@# Link using the static library path directly
	gcc -o $@ x264Client.o $(X264_STATIC_LIB) $(LIBSAFEC_LIB) -lm -lpthread -ldl -lrt -s
	@echo "Copying static x264Client >> $(MTX_APPS) to release folder"
	#@mkdir -p ../../MediaServer/release
	@cp $(MTX_APPS) ../../build
	@cp version.txt ../../build/$(MTX_APPS)-version.txt

x264Client.o: x264Client.c $(VERSION_H)
	gcc -Wno-maybe-uninitialized -Wshadow -O3 -ffast-math -Wall \
	-I. -I$(X264_INC) -I$(LIBSAFEC_INC) \
	-std=gnu99 -D_GNU_SOURCE -D_FORTIFY_SOURCE=2 \
	-fomit-frame-pointer -fno-tree-vectorize -c $< -o $@

clean: 
	@echo "Cleaning project..."
	rm -f *.o $(MTX_APPS) $(VERSION_H) .new_hash .source_hash

# 4. Version generation logic
build_revision:
	@md5sum $(HASH_TARGETS) | md5sum > .new_hash
	@if [ ! -f $(VERSION_TXT) ] || [ ! -f $(VERSION_H) ] || ! cmp -s .new_hash .source_hash; then \
		echo "Version files missing or changes detected. Generating version..."; \
		read -r ma mi bu < $(VERSION_TXT) 2>/dev/null || { ma=1; mi=0; bu=0; }; \
		if [ -f $(VERSION_TXT) ] && [ -f $(VERSION_H) ]; then \
			bu=$$((bu + 1)); \
			if [ $$bu -gt 999 ]; then bu=0; mi=$$((mi + 1)); fi; \
			if [ $$mi -gt 999 ]; then mi=0; ma=$$((ma + 1)); fi; \
		fi; \
		echo "$$ma $$mi $$bu" > $(VERSION_TXT); \
		echo "// Generated file. Do not edit." > $(VERSION_H); \
		echo "#define APP_VERSION \"$$ma.$$mi.$$bu\"" >> $(VERSION_H); \
		echo "#define BUILD_TIMESTAMP \"`date '+%A %d %B %Y %I:%M:%S %p %Z'`\"" >> $(VERSION_H); \
		mv .new_hash .source_hash; \
		echo "App Version: $$ma.$$mi.$$bu"; \
	else \
		echo "No changes detected. Current Version: $$(awk '{print $$1"."$$2"."$$3}' $(VERSION_TXT))"; \
		rm -f .new_hash; \
	fi
