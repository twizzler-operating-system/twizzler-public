#!/usr/bin/env bash


set -e

if [[ "$ARCH" == "" ]]; then
	echo "Set ARCH variable before calling (e.g. x86_64)"
	exit 1
fi

TARGET=$ARCH-pc-twizzler-musl

if [[ "$PROJECT" == "" ]]; then
	echo "Set PROJECT variable before calling (used to determine sysroot; e.g. x86_64)"
	exit 1
fi

if [[ "$PREFIX" == "" ]]; then
	echo "Set PREFIX variable before calling (path to install toolchain to)."
	exit 1
fi

if [ ! -d "$(pwd)/../projects/$PROJECT" ]; then
	echo "Project $PROJECT not found in ../projects (are you running in tools?)"
	exit 1
fi

export PREFIX
export TARGET
export SYSROOT="$(pwd)/../projects/$PROJECT/build/us/sysroot"
export PATH="$PREFIX/bin:$PATH"

if [ ! -f $SYSROOT/usr/include/stdio.h ]; then
	echo "Need to install musl headers into sysroot before running this script (run make sysroot-prep)"
	exit 1
fi



cd build-gcc-hosted

make all-target-libstdc++-v3
make install-target-libstdc++-v3

make all-target-libsanitizer
make install-target-libsanitizer

mkdir -p $SYSROOT/usr/lib
cp -a "$PREFIX/x86_64-pc-twizzler-musl/lib/libstdc++.so*" $SYSROOT/usr/lib/
#cp "$PREFIX/x86_64-pc-twizzler-musl/lib/libsupc++*" $SYSROOT/usr/lib/
