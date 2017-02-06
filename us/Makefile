
RAMDISK_OFILES=test.bin test.fot

OBJCOPY=$(TOOLCHAIN_PREFIX)objcopy
CFLAGS=-Wall -Wextra -std=gnu11 -Og -g -ffreestanding -nostdlib -Iinclude
CC=$(TOOLCHAIN_PREFIX)gcc

BUILDDIR:=../$(BUILDDIR)/us

all: $(BUILDDIR)/initrd.tar

$(BUILDDIR)/initrd.tar: $(addprefix $(BUILDDIR)/,$(RAMDISK_OFILES)) $(RAMDISK_FILES)
	tar -C $(BUILDDIR) -cf $(BUILDDIR)/../initrd.tar $(RAMDISK_OFILES)
	tar -rf $(BUILDDIR)/../initrd.tar $(RAMDISK_FILES)

$(BUILDDIR)/%.bin: %.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@.1 $< -pie -fpic -static -static-libgcc
	$(OBJCOPY) -O binary $@.1 $@

$(BUILDDIR)/%.fot: %.fot.in ../tools/fotgen
	@mkdir -p $(BUILDDIR)
	../tools/fotgen $< -o $@
