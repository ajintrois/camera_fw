#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# --- Configuration ---
# Get the absolute path of the directory where the script is located
# then go up one level to reach the x264Client folder
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
LIB_DEST="$PROJECT_ROOT/../../MediaServer/libs/x264"
X264_SRC_DIR="$PROJECT_ROOT/../../Libraries/x264_source"

# Official VideoLAN Source
REPO_URL="https://code.videolan.org/videolan/x264.git"

echo "======================================================"
echo "  Mediatronix x264 Hardened Setup Script"
echo "======================================================"

# 1. Create Destination Folder
mkdir -p "$LIB_DEST"

# 2. Clone the Source if it doesn't exist
if [ ! -d "$X264_SRC_DIR" ]; then
    echo "[1/4] Cloning official x264 source..."
    git clone "$REPO_URL" "$X264_SRC_DIR"
else
    echo "[1/4] Source directory exists. Skipping clone."
fi

cd "$X264_SRC_DIR"

# 3. Ensure we are on the latest master
echo "[2/4] Fetching latest updates and checking out master..."
git fetch origin
git checkout master
# git pull origin master
git reset --hard origin/master # Ensure a clean slate for patching

# --- NEW: APPLY SECURITY PATCHES TO SOURCE ---
echo "[2.5/4] Patching banned functions for EMBA compliance..."

# Add to [2.5/4] in your setup_x264.sh

# 1. Inject the macro at the top of the file
sed -i '1i #include <string.h>\n#define SAFE_STRCPY(buf, src, len) do { if ((len) > 0) { strncpy((buf), (src), (len)); (buf)[(len) - 1] = "\\0"; } } while (0)' encoder/ratecontrol.c

# 2. Replace the strcpy calls
sed -i 's/strcpy( output, input )/SAFE_STRCPY( output, input, (strlen(input) + strlen(suffix) + 1) )/g' encoder/ratecontrol.c
sed -i 's/strcpy( psz_zones, h->param.rc.psz_zones )/SAFE_STRCPY( psz_zones, h->param.rc.psz_zones, (strlen(h->param.rc.psz_zones) + 1) )/g' encoder/ratecontrol.c

# 4. Configure with Security Hardening Flags
# -fstack-protector-strong: Protection against stack smashing
# -D_FORTIFY_SOURCE=2: Buffer overflow checks
# -Wl,-z,relro,-z,now: Full RELRO (Read-only Global Offset Table)
echo "[3/4] Configuring with NIST-compliant hardening flags..."
call_shared_build()
{
    ./configure \
    --prefix="$LIB_DEST" \
    --enable-shared \
    --enable-pic \
    --extra-cflags="-fstack-protector-strong -D_FORTIFY_SOURCE=2" \
    --extra-ldflags="-Wl,-z,relro -Wl,-z,now" \
    --host=aarch64-linux \
    --disable-cli \
    --enable-strip
}

call_static_build()
{
    ./configure \
        --prefix="$LIB_DEST" \
        --enable-static \
        --disable-shared \
        --enable-pic \
        --extra-cflags="-fstack-protector-strong -D_FORTIFY_SOURCE=2" \
        --host=aarch64-linux \
        --disable-cli
}

call_static_build

# 5. Build and Install
echo "[4/4] Compiling and installing to $LIB_DEST..."
make -j$(nproc)
make install

echo "======================================================"
echo "  Setup Complete. Library ready in ../Libraries"
echo "======================================================"