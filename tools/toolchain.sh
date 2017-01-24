#!/bin/sh

set -e

if [[ "$TARGET" == "" ]]; then
	echo "Set TARGET variable before calling (e.g. x86_64-pc-elf)."
	exit 1
fi

if [[ "$PREFIX" == "" ]]; then
	echo "Set PREFIX variable before calling (path to install toolchain to)."
	exit 1
fi

export PREFIX
export TARGET
export PATH="$PREFIX/bin:$PATH"

BINUTILSVER=2.27
GCCVER=6.3.0

wget ftp://ftp.gnu.org/gnu/binutils/binutils-${BINUTILSVER}.tar.bz2
wget ftp://ftp.gnu.org/gnu/gcc/gcc-${GCCVER}/gcc-${GCCVER}.tar.bz2

tar xf binutils-${BINUTILSVER}.tar.bz2
tar xf gcc-${GCCVER}.tar.bz2

mkdir build-binutils build-gcc
cd build-binutils

../binutils-${BINUTILSVER}/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install

cd ../build-gcc

../gcc-${GCCVER}/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers

make all-gcc all-target-libgcc
if [[ "$TARGET" == "x86_64-pc-elf" ]]; then
	cd $(TARGET)/libgcc
	sed -i 's/^CRTSTUFF_T_CFLAGS =.*$/CRTSTUFF_T_CFLAGS = -mcmodel=kernel/g' Makefile
	make clean
	make
	cd ../..
fi
make install-gcc install-target-libgcc

