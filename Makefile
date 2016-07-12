INCLUDES=-Iinclude

QEMU=../riscv-qemu/riscv-softmmu/qemu-system-riscv
BBL=../toolchain/riscv64-unknown-elf/bin/bbl

CFLAGS=-Wall -Wextra -Wpedantic -std=gnu11 -include stdbool.h -include stddef.h -include stdint.h -I include $(INCLUDES)

all: kernel

test: kernel
	$(QEMU) -kernel $(BBL) -append kernel $(QEMU_FLAGS)

kernel: start.o link.ld memory.o init.o ctx.o
	riscv64-unknown-elf-gcc -ffreestanding -nostdlib start.o ctx.o memory.o init.o -o kernel -T link.ld -lgcc

start.o: start.S
	riscv64-unknown-elf-gcc $(INCLUDES) -c start.S -o start.o

ctx.o: ctx.S
	riscv64-unknown-elf-gcc $(INCLUDES) -c ctx.S -o ctx.o

memory.o: memory.c
	riscv64-unknown-elf-gcc $(CFLAGS) -c memory.c -o memory.o

init.o: init.c
	riscv64-unknown-elf-gcc $(CFLAGS) -c init.c -o init.o

od: kernel
	riscv64-unknown-elf-objdump -d kernel
