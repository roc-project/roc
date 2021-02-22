#! /bin/bash
set -euxo pipefail

TOOLCHAIN="arm-bcm2708hardfp-linux-gnueabi"
COMPILER="gcc-4.7.1-release"
CPU="arm1176" # armv6

scons -Q clean

scons -Q \
    --enable-werror \
    --enable-pulseaudio-modules \
    --enable-tests \
    --enable-examples \
    --build-3rdparty=libuv,libunwind,libatomic_ops,openfec,alsa,pulseaudio:5.0,speexdsp,sox,cpputest \
    --host=${TOOLCHAIN}

find bin/${TOOLCHAIN} -name 'roc-test-*' \
     -not -name 'roc-test-library' |\
    while read t
    do
        LD_LIBRARY_PATH="/opt/sysroot/lib:${PWD}/build/3rdparty/${TOOLCHAIN}/${COMPILER}/rpath" \
            python2 scripts/build/timeout.py 300 \
            qemu-arm -L "/opt/sysroot" -cpu ${CPU} $t
    done
