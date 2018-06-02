CONFIG_DEBUG=y
CONFIG_WERROR=y
CONFIG_OPTIMIZE=0

CONFIG_ARCH=x86_64
CONFIG_MACHINE=pc

CONFIG_UBSAN=n

CONFIG_INSTRUMENT=n

# set this to your toolchain path
TOOLCHAIN_PATH=/home/dbittman/code/twizzler-kernel/.tc/
QEMU_FLAGS+="-enable-kvm"

