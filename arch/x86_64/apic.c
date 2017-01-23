/* We're going to assume, since we're x86_64, that the APIC exists. If
 * it doesn't, buy a better computer.
 */

#include <stdint.h>
#include <arch/x86_64-msr.h>
#include <system.h>
#include <printk.h>
#include <string.h>
#include <memory.h>
#include <debug.h>
#include <thread-bits.h>
#include <processor.h>
#include <arch/x86_64-madt.h>
#include <interrupt.h>
#include <arch/x86_64-io.h>
static uintptr_t lapic_addr = 0;
struct spinlock ipi_lock = SPINLOCK_INIT;

#define LAPIC_DISABLE          0x10000
#define LAPIC_ID                                0x20
#define LAPIC_VER                               0x30
#define LAPIC_TPR                               0x80
#define LAPIC_APR                               0x90
#define LAPIC_PPR                               0xA0
#define LAPIC_EOI                               0xB0
#define LAPIC_LDR                               0xD0
#define LAPIC_DFR                               0xE0
#define LAPIC_SPIV                              0xF0
#define LAPIC_SPIV_ENABLE_APIC  0x100
#define LAPIC_ISR                               0x100
#define LAPIC_TMR                               0x180
#define LAPIC_IRR                               0x200
#define LAPIC_ESR                               0x280
#define LAPIC_ICR                               0x300
#define LAPIC_ICR_DS_SELF               0x40000
#define LAPIC_ICR_DS_ALLINC             0x80000
#define LAPIC_ICR_DS_ALLEX              0xC0000
#define LAPIC_ICR_TM_LEVEL              0x8000
#define LAPIC_ICR_LEVELASSERT   0x4000
#define LAPIC_ICR_STATUS_PEND   0x1000
#define LAPIC_ICR_DM_LOGICAL    0x800
#define LAPIC_ICR_DM_LOWPRI             0x100
#define LAPIC_ICR_DM_SMI                0x200
#define LAPIC_ICR_DM_NMI                0x400
#define LAPIC_ICR_DM_INIT               0x500
#define LAPIC_ICR_DM_SIPI               0x600
#define LAPIC_ICR_SHORT_DEST    0x0
#define LAPIC_ICR_SHORT_SELF    0x1
#define LAPIC_ICR_SHORT_ALL     0x2
#define LAPIC_ICR_SHORT_OTHERS  0x3
#define LAPIC_LVTT                              0x320
#define LAPIC_LVTPC                     0x340
#define LAPIC_LVT0                              0x350
#define LAPIC_LVT1                              0x360
#define LAPIC_LVTE                              0x370
#define LAPIC_TICR                              0x380
#define LAPIC_TCCR                              0x390
#define LAPIC_TDCR                              0x3E0

static inline void lapic_write(int reg, uint32_t data)
{
    *((volatile uint32_t *)(lapic_addr + reg)) = data;
}

static inline uint32_t lapic_read(int reg)
{
	return *(volatile uint32_t *)(lapic_addr + reg);
}

void x86_64_signal_eoi(void)
{
    lapic_write(LAPIC_EOI, 0x0);
}

int arch_processor_current_id(void)
{
	if(!lapic_addr)
		return 0;
	return lapic_read(LAPIC_ID) >> 24;
}

static void set_lapic_timer(unsigned tmp)
{
    /* THE ORDER OF THESE IS IMPORTANT!
     * If you write the initial count before you
     * tell it to set periodic mode and set the vector
     * it will not generate an interrupt! */
    lapic_write(LAPIC_LVTT, 32 | 0x20000);
    lapic_write(LAPIC_TDCR, 3);
    lapic_write(LAPIC_TICR, tmp * 2);
}

static void lapic_configure(int bsp)
{
	for(int i=0;i<=255;i++)
		x86_64_signal_eoi();
	/* these are not yet configured */
	lapic_write(LAPIC_DFR, 0xFFFFFFFF);
	lapic_write(LAPIC_LDR, (lapic_read(LAPIC_LDR)&0x00FFFFFF)|1);
	/* disable the timer while we set up */
	lapic_write(LAPIC_LVTT, LAPIC_DISABLE);

    /* if we accept the extint stuff (the boot processor) we need to not
     * mask, and set the proper flags for these entries.
     * LVT1: NMI
     * LVT0: extINT, level triggered
     */
    lapic_write(LAPIC_LVT1, 0x400 | (bsp ? 0 : LAPIC_DISABLE)); //NMI
    lapic_write(LAPIC_LVT0, 0x8700 | (bsp ? 0 : LAPIC_DISABLE)); //external interrupts
    /* disable errors (can trigger while messing with masking) and performance
     * counter, but also set a non-zero vector */
    lapic_write(LAPIC_LVTE, 0xFF | LAPIC_DISABLE);
    lapic_write(LAPIC_LVTPC, 0xFF | LAPIC_DISABLE);

    /* accept all priority levels */
    lapic_write(LAPIC_TPR, 0);
    /* finally write to the spurious interrupt register to enable
     * the interrupts */
    lapic_write(LAPIC_SPIV, 0x0100 | 0xFF);
	set_lapic_timer(0x1000000); /* TODO: timer calibrate */
}

__initializer void x86_64_lapic_init_percpu(void)
{
	/* enable the LAPIC */
	uint32_t lo, hi;
	x86_64_rdmsr(X86_MSR_APIC_BASE, &lo, &hi);
	uintptr_t paddr = ((((uintptr_t)hi & 0xF) << 32) | lo) & ~0xFFF;

	lo |= X86_MSR_APIC_BASE_ENABLE;
	x86_64_wrmsr(X86_MSR_APIC_BASE, lo, hi);

	lapic_addr = paddr + PHYSICAL_MAP_START;
	lapic_configure(lo & X86_MSR_APIC_BASE_BSP);
}

static void x86_cpu_send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v)
{
    assert((v & LAPIC_ICR_DM_INIT) || (v & LAPIC_ICR_LEVELASSERT));
    int send_status;
    int old = arch_interrupt_set(0);
    spinlock_acquire(&ipi_lock);
    /* Writing to the lower ICR register causes the interrupt
     * to get sent off (Intel 3A 10.6.1), so do the higher reg first */
    lapic_write(LAPIC_ICR+0x10, (dst << 24));
    unsigned lower = v | (dest_shorthand << 18);
    /* gotta have assert for all except init */
    lapic_write(LAPIC_ICR, lower);
    /* Wait for send to finish */
    do {
        asm("pause");
        send_status = lapic_read(LAPIC_ICR) & LAPIC_ICR_STATUS_PEND;
    } while (send_status);
    spinlock_release(&ipi_lock);
    arch_interrupt_set(old);
}

void x86_64_processor_send_ipi(long dest, int signal)
{
	x86_cpu_send_ipi(dest == PROCESSOR_IPI_DEST_OTHERS
				? LAPIC_ICR_SHORT_OTHERS : LAPIC_ICR_SHORT_DEST,
			dest == PROCESSOR_IPI_DEST_OTHERS ? 0 : dest,
			LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | signal);
}

void x86_64_processor_halt_others(void)
{
	x86_64_processor_send_ipi(PROCESSOR_IPI_DEST_OTHERS, PROCESSOR_IPI_HALT);
}

void arch_panic_begin(void)
{
	x86_64_processor_halt_others();
}

static void wait(int ms)
{
	for(int i=0;i<ms;i++) {
		x86_64_outb(0x43, 0x30);
		x86_64_outb(0x40, 0xA9);
		x86_64_outb(0x40, 0x04);

		x86_64_outb(0x43, 0xE2);
		int status;
		do {
			status = x86_64_inb(0x40);
			asm("pause");
		} while(!(status & (1 << 7)));
	}
}

#define BIOS_RESET_VECTOR 0x467
#define CMOS_RESET_CODE 0xF
#define CMOS_RESET_JUMP 0xa
#define RM_GDT_SIZE 0x18
#define GDT_POINTER_SIZE 0x6
#define RM_GDT_START 0x7100
#define BOOTFLAG_ADDR 0x7200

#ifdef __clang__
__attribute__((no_sanitize("alignment")))
#else
__attribute__((no_sanitize_undefined))
#endif
static inline void write_bios_reset(uintptr_t addr)
{
	*((volatile uint32_t *)(BIOS_RESET_VECTOR + PHYSICAL_MAP_START)) = ((addr & 0xFF000) << 12);
}

extern int trampoline_start, trampoline_end, rm_gdt, pmode_enter, rm_gdt_pointer;
void arch_processor_boot(struct processor *proc)
{
	proc->arch.kernel_stack = (void *)mm_virtual_alloc(KERNEL_STACK_SIZE, PM_TYPE_ANY, true);
	printk("Poking secondary CPU %ld, proc->arch.kernel_stack = %lx (proc=%p)\n", proc->id, proc->arch.kernel_stack, proc);
	*(void **)(proc->arch.kernel_stack + KERNEL_STACK_SIZE - sizeof(void *)) = proc;

	uintptr_t bootaddr_phys = 0x7000;

	lapic_write(LAPIC_ESR, 0);
	write_bios_reset(bootaddr_phys);
	x86_64_cmos_write(CMOS_RESET_CODE, CMOS_RESET_JUMP);

	size_t trampoline_size = (uintptr_t)&trampoline_end - (uintptr_t)&trampoline_start;
	memcpy((void *)(0x7000 + PHYSICAL_MAP_START), &trampoline_start, trampoline_size);
	memcpy((void *)(RM_GDT_START + GDT_POINTER_SIZE + PHYSICAL_MAP_START),
			&rm_gdt, RM_GDT_SIZE);
	memcpy((void *)(RM_GDT_START + PHYSICAL_MAP_START),
			&rm_gdt_pointer, GDT_POINTER_SIZE);
	memcpy((void *)(0x7200 + PHYSICAL_MAP_START), &pmode_enter, 0x100);

	*(volatile uintptr_t *)(0x7300 + PHYSICAL_MAP_START) = proc->arch.kernel_stack + KERNEL_STACK_SIZE;
	asm volatile("mfence" ::: "memory");
	lapic_write(LAPIC_ESR, 0);
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);

	wait(100);
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);

	wait(100);
	for(int i=0;i<3;i++) {
		x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_DM_SIPI | ((bootaddr_phys >> 12) & 0xFF));
		wait(10);
	}

	int timeout;
	for(timeout=10000;timeout>0;timeout--) {
		if(*(volatile _Atomic uintptr_t *)(0x7300) == 0)
			break;
		wait(10);
	}

	if(timeout == 0) {
		printk("failed to start CPU %d\n", proc->id);
	}
}

