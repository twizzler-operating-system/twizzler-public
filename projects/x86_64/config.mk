CONFIG_DEBUG=n
CONFIG_WERROR=y
CONFIG_OPTIMIZE=3

CONFIG_ARCH=x86_64
CONFIG_MACHINE=pc

CONFIG_UBSAN=y

CONFIG_INSTRUMENT=n

# set this to your toolchain path
TOOLCHAIN_PATH=/home/dbittman/code/twizzler-kernel/.toolchain
QEMU_FLAGS+="-enable-kvm"
