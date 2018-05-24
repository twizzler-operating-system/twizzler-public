#pragma once
#define PROCESSOR_IPI_DEST_OTHERS -1
#define PROCESSOR_IPI_SHOOTDOWN 100
#define PROCESSOR_IPI_HALT 90
struct x86_64_tss
{
	uint32_t prev_tss;
	uint64_t esp0;       // The stack pointer to load when we change to kernel mode.
	uint32_t ss0;        // The stack segment to load when we change to kernel mode.
	uint64_t esp1;       // Unused...
	uint32_t ss1;
	uint64_t esp2;
	uint32_t ss2;
	uint64_t cr3;
	uint64_t rip;
	uint64_t rflags;
	uint64_t rax;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint32_t es;         // The value to load into ES when we change to kernel mode.
	uint32_t cs;         // The value to load into CS when we change to kernel mode.
	uint32_t ss;         // The value to load into SS when we change to kernel mode.
	uint32_t ds;         // The value to load into DS when we change to kernel mode.
	uint32_t fs;         // The value to load into FS when we change to kernel mode.
	uint32_t gs;         // The value to load into GS when we change to kernel mode.
	uint32_t ldt;        // Unused...
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed));

struct x86_64_gdt_entry
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t  base_middle; 
	uint8_t  access;
	uint8_t  granularity;
	uint8_t  base_high; 
} __attribute__((packed));

struct idt_entry {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t __pad0;
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

struct virtexcep_info {
	uint32_t exitreason;
	uint32_t res1;
	uint64_t exitqual;
	uint64_t guest_linear;
	uint64_t guest_phys;
	uint16_t eptp_idx;
} __attribute__((packed));

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
	uintptr_t vmcs;
	uint64_t vcpu_state_regs[NUM_REGS];
	struct virtexcep_info *virtexcep_info;
	int launched;
	uint32_t revid;
};

_Static_assert(offsetof(struct arch_processor, scratch_sp) == 0, "scratch_sp offset must be 0 (or update offsets in gate.S)");
_Static_assert(offsetof(struct arch_processor, tcb) == 8, "tcb offset must be 8 (or update offsets in gate.S)");
_Static_assert(offsetof(struct arch_processor, kernel_stack) == 16, "kernel_stack offset must be 16 (or update offsets in gate.S)");

__attribute__((const)) static inline struct thread * __x86_64_get_current_thread(void)
{
	uint64_t tmp;
	asm ("movq %%gs:%c[curr], %0" : "=r"(tmp) : [curr]"i"(offsetof(struct arch_processor, curr)));
	return (void *)tmp;
}

#define current_thread __x86_64_get_current_thread()

__noinstrument
static inline void arch_processor_relax(void)
{
	asm volatile("pause");
}

__noinstrument
static inline void arch_processor_halt(void)
{
	asm volatile("hlt");
}

__noinstrument
static inline unsigned long long arch_processor_timestamp(void)
{
    unsigned int lo, hi;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));                        
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );  
}

struct processor;
void x86_64_processor_post_vm_init(struct processor *proc);

