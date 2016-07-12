INCLUDES=-Iinclude

QEMU=../riscv-qemu/riscv-softmmu/qemu-system-riscv
BBL=../toolchain/riscv64-unknown-elf/bin/bbl

TOOLCHAIN_PREFIX=riscv64-unknown-linux-gnu-

CFLAGS=-Wall -Wextra -Wpedantic -std=gnu11 -include stdbool.h -include stddef.h -include stdint.h -I include $(INCLUDES) -include printk.h

C_SOURCES=memory.c init.c interrupt.c main.c vsprintk.c
ASM_SOURCES=start.S ctx.S

OBJECTS=$(ASM_SOURCES:.S=.o) $(C_SOURCES:.c=.o)

all: kernel

test: kernel
	$(QEMU) -kernel $(BBL) -append kernel $(QEMU_FLAGS) -serial stdio

kernel: link.ld $(OBJECTS)
	$(TOOLCHAIN_PREFIX)gcc -ffreestanding -nostdlib $(OBJECTS) -o kernel -T link.ld -lgcc

%.o : %.S
	$(TOOLCHAIN_PREFIX)gcc $(INCLUDES) -c $< -o $@

%.o : %.c
	$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c $< -o $@

clean:
	-rm $(OBJECTS) kernel

od: kernel
	$(TOOLCHAIN_PREFIX)objdump -d kernel

re: kernel
	$(TOOLCHAIN_PREFIX)readelf -a kernel
