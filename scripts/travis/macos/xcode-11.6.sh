#! /bin/bash
set -euxo pipefail

brew update || true

brew unlink python@2
brew list |\
    grep -vE 'pkg-config|automake|libtool|cmake|xz|readline|openssl|sqlite|python|gdbm|xquartz' |\
    xargs brew pin

brew link --overwrite python@3.9

brew install "scons"
brew install "ragel"
brew install "gengetopt"
brew install "libuv"
brew install "speexdsp"
brew install "sox"
brew install "cpputest"
brew install "google-benchmark"

scons -Q clean

scons -Q \
      --enable-werror \
      --enable-debug \
      --enable-tests \
      --enable-benchmarks \
      --enable-examples \
      --sanitizers=all \
      --build-3rdparty=openfec \
      test

scons -Q \
      --enable-werror \
      --enable-tests \
      --enable-benchmarks \
      --enable-examples \
      --build-3rdparty=openfec \
      test

scons -Q \
      --enable-werror \
      --enable-tests \
      --enable-benchmarks \
      --enable-examples \
      --build-3rdparty=all \
      test
