#include <stdint.h>
#include <string.h>
#include <memory.h>
#include <processor.h>
#define TYPE_INT_KERNEL 0x8E
#define TYPE_INT_USER   0xEE

struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t __pad0;
	uint8_t type;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t __pad1;
} __attribute__((packed));

static _Alignas(16) struct idt_entry idt[256];

static _Alignas(16) struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idt_ptr;

static void idt_set_gate(int vector, uintptr_t base, uint8_t flags)
{
	idt[vector].offset_low  = base & 0xFFFF;
	idt[vector].offset_mid  = (base >> 16) & 0xFFFF;
	idt[vector].offset_high = (base >> 32) & 0xFFFFFFFF;
	idt[vector].selector = 0x08;
	idt[vector].type = flags;
	idt[vector].__pad0 = 0;
	idt[vector].__pad1 = 0;
}

extern void (*x86_64_isr0)(void);
extern void (*x86_64_isr1)(void);
extern void (*x86_64_isr2)(void);
extern void (*x86_64_isr3)(void);
extern void (*x86_64_isr4)(void);
extern void (*x86_64_isr5)(void);
extern void (*x86_64_isr6)(void);
extern void (*x86_64_isr7)(void);
extern void (*x86_64_isr8)(void);
extern void (*x86_64_isr9)(void);
extern void (*x86_64_isr10)(void);
extern void (*x86_64_isr11)(void);
extern void (*x86_64_isr12)(void);
extern void (*x86_64_isr13)(void);
extern void (*x86_64_isr14)(void);
extern void (*x86_64_isr15)(void);
extern void (*x86_64_isr16)(void);
extern void (*x86_64_isr17)(void);
extern void (*x86_64_isr18)(void);
extern void (*x86_64_isr19)(void);
extern void (*x86_64_isr20)(void);
extern void (*x86_64_isr21)(void);
extern void (*x86_64_isr22)(void);
extern void (*x86_64_isr23)(void);
extern void (*x86_64_isr24)(void);
extern void (*x86_64_isr25)(void);
extern void (*x86_64_isr26)(void);
extern void (*x86_64_isr27)(void);
extern void (*x86_64_isr28)(void);
extern void (*x86_64_isr29)(void);
extern void (*x86_64_isr30)(void);
extern void (*x86_64_isr31)(void);
extern void (*x86_64_isr32)(void);
extern void (*x86_64_isr33)(void);
extern void (*x86_64_isr34)(void);
extern void (*x86_64_isr35)(void);
extern void (*x86_64_isr36)(void);
extern void (*x86_64_isr37)(void);
extern void (*x86_64_isr38)(void);
extern void (*x86_64_isr39)(void);
extern void (*x86_64_isr40)(void);
extern void (*x86_64_isr41)(void);
extern void (*x86_64_isr42)(void);
extern void (*x86_64_isr43)(void);
extern void (*x86_64_isr44)(void);
extern void (*x86_64_isr45)(void);
extern void (*x86_64_isr46)(void);
extern void (*x86_64_isr47)(void);
extern void (*x86_64_isr_ignore)(void);
extern void (*x86_64_isr_shootdown)(void);
extern void (*x86_64_isr_halt)(void);
void idt_init(void)
{
	memset(idt, 0, sizeof(idt));
	idt_set_gate(0, (uintptr_t)&x86_64_isr0, TYPE_INT_KERNEL);
	idt_set_gate(1, (uintptr_t)&x86_64_isr1, TYPE_INT_KERNEL);
	idt_set_gate(2, (uintptr_t)&x86_64_isr2, TYPE_INT_KERNEL);
	idt_set_gate(3, (uintptr_t)&x86_64_isr3, TYPE_INT_KERNEL);
	idt_set_gate(4, (uintptr_t)&x86_64_isr4, TYPE_INT_KERNEL);
	idt_set_gate(5, (uintptr_t)&x86_64_isr5, TYPE_INT_KERNEL);
	idt_set_gate(6, (uintptr_t)&x86_64_isr6, TYPE_INT_KERNEL);
	idt_set_gate(7, (uintptr_t)&x86_64_isr7, TYPE_INT_KERNEL);
	idt_set_gate(8, (uintptr_t)&x86_64_isr8, TYPE_INT_KERNEL);
	idt_set_gate(9, (uintptr_t)&x86_64_isr9, TYPE_INT_KERNEL);
	idt_set_gate(10, (uintptr_t)&x86_64_isr10, TYPE_INT_KERNEL);
	idt_set_gate(11, (uintptr_t)&x86_64_isr11, TYPE_INT_KERNEL);
	idt_set_gate(12, (uintptr_t)&x86_64_isr12, TYPE_INT_KERNEL);
	idt_set_gate(13, (uintptr_t)&x86_64_isr13, TYPE_INT_KERNEL);
	idt_set_gate(14, (uintptr_t)&x86_64_isr14, TYPE_INT_KERNEL);
	idt_set_gate(15, (uintptr_t)&x86_64_isr15, TYPE_INT_KERNEL);
	idt_set_gate(16, (uintptr_t)&x86_64_isr16, TYPE_INT_KERNEL);
	idt_set_gate(17, (uintptr_t)&x86_64_isr17, TYPE_INT_KERNEL);
	idt_set_gate(18, (uintptr_t)&x86_64_isr18, TYPE_INT_KERNEL);
	idt_set_gate(19, (uintptr_t)&x86_64_isr19, TYPE_INT_KERNEL);
	idt_set_gate(20, (uintptr_t)&x86_64_isr20, TYPE_INT_KERNEL);
	idt_set_gate(21, (uintptr_t)&x86_64_isr21, TYPE_INT_KERNEL);
	idt_set_gate(22, (uintptr_t)&x86_64_isr22, TYPE_INT_KERNEL);
	idt_set_gate(23, (uintptr_t)&x86_64_isr23, TYPE_INT_KERNEL);
	idt_set_gate(24, (uintptr_t)&x86_64_isr24, TYPE_INT_KERNEL);
	idt_set_gate(25, (uintptr_t)&x86_64_isr25, TYPE_INT_KERNEL);
	idt_set_gate(26, (uintptr_t)&x86_64_isr26, TYPE_INT_KERNEL);
	idt_set_gate(27, (uintptr_t)&x86_64_isr27, TYPE_INT_KERNEL);
	idt_set_gate(28, (uintptr_t)&x86_64_isr28, TYPE_INT_KERNEL);
	idt_set_gate(29, (uintptr_t)&x86_64_isr29, TYPE_INT_KERNEL);
	idt_set_gate(30, (uintptr_t)&x86_64_isr30, TYPE_INT_KERNEL);
	idt_set_gate(31, (uintptr_t)&x86_64_isr31, TYPE_INT_KERNEL);
	idt_set_gate(32, (uintptr_t)&x86_64_isr32, TYPE_INT_KERNEL);
	idt_set_gate(33, (uintptr_t)&x86_64_isr33, TYPE_INT_KERNEL);
	idt_set_gate(34, (uintptr_t)&x86_64_isr34, TYPE_INT_KERNEL);
	idt_set_gate(35, (uintptr_t)&x86_64_isr35, TYPE_INT_KERNEL);
	idt_set_gate(36, (uintptr_t)&x86_64_isr36, TYPE_INT_KERNEL);
	idt_set_gate(37, (uintptr_t)&x86_64_isr37, TYPE_INT_KERNEL);
	idt_set_gate(38, (uintptr_t)&x86_64_isr38, TYPE_INT_KERNEL);
	idt_set_gate(39, (uintptr_t)&x86_64_isr39, TYPE_INT_KERNEL);
	idt_set_gate(40, (uintptr_t)&x86_64_isr40, TYPE_INT_KERNEL);
	idt_set_gate(41, (uintptr_t)&x86_64_isr41, TYPE_INT_KERNEL);
	idt_set_gate(42, (uintptr_t)&x86_64_isr42, TYPE_INT_KERNEL);
	idt_set_gate(43, (uintptr_t)&x86_64_isr43, TYPE_INT_KERNEL);
	idt_set_gate(44, (uintptr_t)&x86_64_isr44, TYPE_INT_KERNEL);
	idt_set_gate(45, (uintptr_t)&x86_64_isr45, TYPE_INT_KERNEL);
	idt_set_gate(46, (uintptr_t)&x86_64_isr46, TYPE_INT_KERNEL);
	idt_set_gate(47, (uintptr_t)&x86_64_isr47, TYPE_INT_KERNEL);
	
	idt_set_gate(0xFF, (uintptr_t)&x86_64_isr_ignore, TYPE_INT_KERNEL);
	idt_set_gate(PROCESSOR_IPI_SHOOTDOWN, (uintptr_t)&x86_64_isr_shootdown, TYPE_INT_KERNEL);
	idt_set_gate(PROCESSOR_IPI_HALT, (uintptr_t)&x86_64_isr_halt, TYPE_INT_KERNEL);

	idt_ptr.base = (uintptr_t)idt;
	idt_ptr.limit = 256 * sizeof(struct idt_entry) - 1;

	asm volatile("lidt (%0)"::"r"(&idt_ptr) : "memory");
}

void idt_init_secondary(void)
{
	asm volatile("lidt (%0)"::"r"(&idt_ptr) : "memory");
}

