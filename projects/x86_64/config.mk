CONFIG_DEBUG=n
CONFIG_WERROR=y
CONFIG_OPTIMIZE=g

CONFIG_ARCH=x86_64
CONFIG_MACHINE=pc

CONFIG_UBSAN=y

#CONFIG_INSTRUMENT=y

# set this to your toolchain path
TOOLCHAIN_PATH=/home/dbittman/code/twizzler-kernel/.toolchains/x86_64
QEMU_FLAGS+="-enable-kvm"
