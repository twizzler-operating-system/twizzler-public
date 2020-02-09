#pragma once

#define PROCESSOR_IPI_DEST_OTHERS -1
#define PROCESSOR_IPI_SHOOTDOWN 100
#define PROCESSOR_IPI_HALT 90
#define PROCESSOR_IPI_RESUME 89

#define X86_DOUBLE_FAULT_IST_IDX 0

#ifndef ASSEMBLY

struct x86_64_tss {
	uint32_t __res0;
	uint64_t rsp0;
	uint64_t rsp1;
	uint64_t rsp2;

	uint64_t __res1;

	uint64_t ist[7];

	uint64_t __res2;
	uint16_t _res3;
	uint16_t iomap_offset;
} __attribute__((packed));

struct x86_64_gdt_entry {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed));

struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t ist;
	uint8_t type;
	uint16_t offset_mid;
	uint32_t offset_high;
	uint32_t __pad1;
} __attribute__((packed));
extern struct idt_entry idt[256];

enum {
	REG_RAX,
	REG_RBX,
	REG_RCX,
	REG_RDX,
	REG_RDI,
	REG_RSI,
	REG_RBP,
	REG_R8,
	REG_R9,
	REG_R10,
	REG_R11,
	REG_R12,
	REG_R13,
	REG_R14,
	REG_R15,
	REG_CR2,
	HOST_RSP,
	PROC_PTR,
	NUM_REGS,
};

struct thread;
struct arch_processor {
	void *scratch_sp;
	void *tcb;
	void *kernel_stack, *hyper_stack;
	struct thread *curr;
	_Alignas(16) struct x86_64_tss tss;
	_Alignas(16) struct x86_64_gdt_entry gdt[8];
	_Alignas(16) struct __attribute__((packed)) {
		uint16_t limit;
		uint64_t base;
	} gdtptr;
	uintptr_t vmcs, vmxon_region;
	uint64_t vcpu_state_regs[NUM_REGS];
	int launched;
	uint32_t revid;
	struct __packed {
		uint32_t reason;
		_Atomic uint32_t lock;
		uint64_t qual;
		uint64_t linear;
		uint64_t physical;
		uint16_t eptidx;
	} * veinfo;
	uintptr_t *eptp_list;
};

_Static_assert(offsetof(struct arch_processor, scratch_sp) == 0,
  "scratch_sp offset must be 0 (or update offsets in gate.S)");
_Static_assert(offsetof(struct arch_processor, tcb) == 8,
  "tcb offset must be 8 (or update offsets in gate.S)");
_Static_assert(offsetof(struct arch_processor, kernel_stack) == 16,
  "kernel_stack offset must be 16 (or update offsets in gate.S)");

__attribute__((const, always_inline)) static inline struct thread *__x86_64_get_current_thread(void)
{
	uint64_t tmp;
	asm("movq %%gs:%c[curr], %0" : "=r"(tmp) : [ curr ] "i"(offsetof(struct arch_processor, curr)));
	return (void *)tmp;
}

#define current_thread __x86_64_get_current_thread()

/* TODO: use clwb if we can */
#define arch_processor_clwb(x) ({ asm volatile("clflush %0" ::"m"(x) : "memory"); })

__noinstrument static inline void arch_processor_relax(void)
{
	asm volatile("pause");
}

__noinstrument static inline void arch_processor_halt(void)
{
	// asm volatile("mwait" ::"a"(0), "c"(0) : "memory");
	asm volatile("hlt");
	// for(long i = 0; i < 10000000; i++) {
	//	asm volatile("pause");
	//}
}

__noinstrument static inline unsigned long long arch_processor_timestamp(void)
{
	unsigned int lo, hi;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

struct processor;
void x86_64_processor_post_vm_init(struct processor *proc);

#include <cpuid.h>
#include <debug.h>

static inline uint64_t x86_64_cpuid(uint32_t x, uint32_t subleaf, int rnum)
{
	uint32_t regs[4];
	if(!__get_cpuid_count(x, subleaf, &regs[0], &regs[1], &regs[2], &regs[3])) {
		panic("Invalid CPUID issued");
	}
	return regs[rnum];
}
#endif
