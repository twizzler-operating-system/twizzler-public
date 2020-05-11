#!/bin/sh

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

BINUTILSVER=2.32
GCCVER=9.2.0

wget ftp://ftp.gnu.org/gnu/binutils/binutils-${BINUTILSVER}.tar.bz2 -O binutils-${BINUTILSVER}.tar.bz2
wget ftp://ftp.gnu.org/gnu/gcc/gcc-${GCCVER}/gcc-${GCCVER}.tar.xz -O gcc-${GCCVER}.tar.xz

echo Cleaning and extracting sources

[ -d binutils-$BINUTILSVER ] && rm -r binutils-$BINUTILSVER
[ -d gcc-$GCCVER ] && rm -r gcc-$GCCVER

tar xf binutils-${BINUTILSVER}.tar.bz2
tar xf gcc-${GCCVER}.tar.xz

echo Patching sources

cd binutils-$BINUTILSVER
patch -p1 < ../binutils-$BINUTILSVER-twizzler-hosted.patch
cd ../gcc-$GCCVER
patch -p1 < ../gcc-$GCCVER-twizzler-hosted.patch
cd ..


[ -d build-binutils-hosted ] && rm -r build-binutils-hosted
#[ -d build-gcc-hosted ] && rm -r build-gcc-hosted

mkdir -p build-binutils-hosted build-gcc-hosted
cd build-binutils-hosted

../binutils-${BINUTILSVER}/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot="$SYSROOT" --disable-nls --disable-werror --enable-shared
make -j6
make install

exit
cd ../build-gcc-hosted

../gcc-${GCCVER}/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --with-sysroot="$SYSROOT" --enable-shared --enable-threads=posix

make -j6 all-gcc
make install-gcc


