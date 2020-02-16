#pragma once

#define MAX_INTERRUPT_VECTORS 256

void x86_64_ipi_tlb_shootdown(void);
void x86_64_ipi_resume(void);
void x86_64_ipi_halt(void);
void x86_64_signal_eoi(void);

struct x86_64_exception_frame;
void x86_64_exception_entry(struct x86_64_exception_frame *frame, bool was_userspace, bool ignored);
struct x86_64_syscall_frame;
void x86_64_syscall_entry(struct x86_64_syscall_frame *frame);

static inline bool arch_interrupt_set(bool on)
{
	register long old;
	asm volatile("pushfq; pop %0;" : "=r"(old)::"memory"); /* need full barrier for sync */
	if(on)
		asm volatile("mfence; sti" ::: "memory");
	else
		asm volatile("cli; mfence" ::: "memory");
	return !!(old & (1 << 9));
}

#define X86_64_MSI_DM_PHYSICAL 0
#define X86_64_MSI_DM_LOGICAL (1 << 2)
#define X86_64_MSI_RH (1 << 3)

#define x86_64_msi_addr(did, flags) ({ (0xfeeul << 20) | (did << 12) | flags; })

#define X86_64_MSI_EDGE 0ul
#define X86_64_MSI_LEVEL (1ul << 15)
#define X86_64_MSI_LEVEL_ASSERT (1ul << 14)
#define X86_64_MSI_LEVEL_DEASSERT (0)
#define X86_64_MSI_DELIVERY_FIXED 0
#define X86_64_MSI_DELIVERY_LOW (1ul << 8)
