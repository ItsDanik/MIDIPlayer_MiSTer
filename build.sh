#!/bin/sh
# Build MIDI Player for the MiSTer in the toolchain container: our own program
# against the hybrid library (midiplayer/, no SDL2).
# usage: ./build.sh [mister|host] [Release|RelWithDebInfo|Debug]
#   mister  ARM binary for the MiSTer     -> build/mister/midiplayer
#   host    same code for this PC, to run against hybrid/tools/fakecore
set -e
cd "$(dirname "$0")"
TARGET=${1:-mister}
TYPE=${2:-Release}
ROOT=$PWD

CROSS=
if [ "$TARGET" = mister ]; then
    CROSS="-DCMAKE_TOOLCHAIN_FILE=$ROOT/hybrid/toolchain/mister.cmake"
    # the compiler's runtime is linked in, the binary only needs the MiSTer's glibc
    CROSS="$CROSS -DCMAKE_EXE_LINKER_FLAGS='-no-pie -static-libstdc++ -static-libgcc'"
fi

hybrid/docker.sh sh -c "
  cmake -S midiplayer -B build/$TARGET -DCMAKE_BUILD_TYPE=$TYPE $CROSS &&
  cmake --build build/$TARGET -j\$(nproc)"
ls -la "build/$TARGET/midiplayer"

# The launch script's messages on the core's picture (hybrid/tools/mh_message.c)
if [ "$TARGET" = mister ]; then
    hybrid/docker.sh arm-linux-gnueabihf-gcc -O2 -static -Ihybrid/hps -o build/mister/mh_message \
        hybrid/tools/mh_message.c hybrid/hps/*.c -lpthread -lm
fi
