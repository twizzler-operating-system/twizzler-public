#!/bin/sh

set -e

if [ ! -d projects/$PROJECT ]; then
	echo Project $PROJECT not found. Are you in the twizzler root directory?
	exit 1
fi

echo "Welcome to the Twizzler build system. This is a VERY involved process, and I do not recommend
it unless you have some Unix experience."
echo 
echo "The build system will perform the following actions:"
echo "  - Compile a freestanding toolchain for kernel compilation."
echo "  - Compile a temporary C library for compiling a hosted toolchain."
echo "  - Compile a hosted toolchain for userspace program compilation."
echo "  - Compile the Twizzler kernel."
echo "  - Compile the Twizzler drivers and utilities."
echo "  - Compile the ports systems (includes ported programs)."
echo
echo "Note that this means we will compile binutils and gcc twice, each. This should not be done on
a system with limited disk space or memory; gcc in particular can take a while. I recommend you get
yourself a nice cup of tea while you're waiting."
echo
echo "If you've read this, and wish to continue, press enter."
read

echo
echo "Okay :) let's first setup the default project config file. The project you're working on is
$PROJECT. The toolchains will be installed to \`$(pwd)/.tc'. You can change this by editing
projects/$PROJECT/config.mk. Press enter to continue."
read

sed -i "s|TOOLCHAIN_PATH=|TOOLCHAIN_PATH=$(pwd)/.tc|g" projects/$PROJECT/config.mk
