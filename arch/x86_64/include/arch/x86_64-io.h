#pragma once
#include <stdint.h>
static inline void x86_64_outb(uint16_t port, uint8_t val)
{
	asm volatile ("outb %1, %0" : : "dN" (port), "a" ((unsigned char)val));
}

static inline uint8_t x86_64_inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}

static inline void x86_64_outl(uint16_t port, uint32_t val)
{
	asm volatile ("outl %1, %0" : : "dN" (port), "a" (val));
}

static inline uint32_t x86_64_inl(uint16_t port)
{
	uint32_t ret;
	asm volatile("inl %1, %0" : "=a" (ret) : "dN" (port));
	return ret;
}


static inline unsigned char x86_64_cmos_read(unsigned char addr)
{
	unsigned char ret;
	x86_64_outb(0x70,addr);
	asm volatile ("jmp 1f; 1: jmp 1f;1:");
	ret = x86_64_inb(0x71);
	asm volatile ("jmp 1f; 1: jmp 1f;1:");
	return ret;
}

static inline void x86_64_cmos_write(unsigned char addr, unsigned int value)
{
	x86_64_outb(0x70, addr);
	asm volatile ("jmp 1f; 1: jmp 1f;1:");
	x86_64_outb(0x71, value);
	asm volatile ("jmp 1f; 1: jmp 1f;1:");
}

