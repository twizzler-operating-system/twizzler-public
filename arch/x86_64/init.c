#include <machine/pc-multiboot.h>
#include <machine/memory.h>
#include <debug.h>
#include <init.h>
#include <arch/x86_64-msr.h>
#include <arch/x86_64-vmx.h>
#include <processor.h>
#include <string.h>

/* TODO (major): clean up this file */

void serial_init();

extern void _init();
uint64_t x86_64_top_mem;
uint64_t x86_64_bot_mem;
extern void idt_init(void);
extern void idt_init_secondary(void);

static struct multiboot *mb;

static void proc_init(void)
{
	uint64_t cr0, cr4;
	asm volatile("finit");
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= (1 << 2);
	cr0 |= (1 << 5);
	cr0 |= (1 << 1);
	cr0 |= (1 << 16);
	cr0 &= ~(1 << 30); // make sure caching is on
	cr0 &= ~(1 << 29); // make sure caching is on
	asm volatile("mov %0, %%cr0" :: "r"(cr0));
	
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1 << 7); //enable page global TODO: get this to work
	cr4 |= (1 << 10); //enable fast fxsave etc, sse
	cr4 &= ~(1 << 9);
	asm volatile("mov %0, %%cr4" :: "r"(cr4));

	/* enable fast syscall extension */
	uint32_t lo, hi;
	x86_64_rdmsr(X86_MSR_EFER, &lo, &hi);
	lo |= X86_MSR_EFER_SYSCALL | X86_MSR_EFER_NX;
	x86_64_wrmsr(X86_MSR_EFER, lo, hi);
	printk("cr0: %lx, cr4: %lx, efer: %x,%x\n",
			cr0, cr4, hi, lo);

	/* TODO: verify that this setup is "reasonable" */
	x86_64_rdmsr(X86_MSR_MTRRCAP, &lo, &hi);
	int mtrrcnt = lo & 0xFF;
	x86_64_rdmsr(X86_MSR_MTRR_DEF_TYPE, &lo, &hi);
	for(int i=0;i<mtrrcnt;i++) {
		x86_64_rdmsr(X86_MSR_MTRR_PHYSBASE(i), &lo, &hi);
		x86_64_rdmsr(X86_MSR_MTRR_PHYSMASK(i), &lo, &hi);
	}
}

struct ustar_header {
        char name[100];
        char mode[8];
        char uid[8];
        char gid[8];
        char size[12];
        char mtime[12];
        char checksum[8];
        char typeflag[1];
        char linkname[100];
        char magic[6];
        char version[2];
        char uname[32];
        char gname[32];
        char devmajor[8];
        char devminor[8];
        char prefix[155];
        char pad[12];
};

#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x) - PHYS_ADDR_DELTA)
extern int kernel_end;
#include <object.h>
static void x86_64_initrd(void *u)
{
	(void)u;
	static int __id = 0;
	printk("%d mods\n", mb->mods_count);
	if(mb->mods_count == 0) return;
	struct mboot_module *m = mm_ptov(mb->mods_addr);
	struct ustar_header *h = mm_ptov(m->start);
	char *start = (char *)h;
	printk(":: %s\n", h->magic);
	size_t len = m->end - m->start;
	while((char *)h < start + len) {
		char *name = h->name;
		if(!*name) break;
		if(strncmp(h->magic, "ustar", 5)) break;
		char *data = (char *)h+512;
		size_t len = strtol(h->size, NULL, 8);
		size_t reclen = (len + 511) & ~511;

		switch(h->typeflag[0]) {
			case '0': case '7':
				printk("Loading object: %s\n", name);
				int off = 0x400000;
				if(!strncmp(name, "sys", 3)) off = 0x1000; //HACK
				obj_create(++__id, reclen, off);
				struct object *obj = obj_lookup(__id);
				assert(obj != NULL);
				size_t idx = 0;
				for(size_t s = 0;s<reclen;s+=mm_page_size(0),idx++) {
					uintptr_t phys = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
					size_t thislen = 0x1000;
					if((reclen - s) < thislen)
						thislen = reclen - s;
					memcpy(mm_ptov(phys), data + s, thislen);
					obj_cache_page(obj, idx, phys);
				}
				break;
			default:
				printk("Unknown file type in tar archive: %c %s\n", h->typeflag[0], name);
		}

		h = (struct ustar_header *)((char *)h + 512 + reclen);
	}
}
POST_INIT(x86_64_initrd);

void kernel_early_init(void);
void kernel_init(void);
void x86_64_lapic_init_percpu(void);
void x86_64_init(struct multiboot *mth)
{
	mb = mth;
	idt_init();
	serial_init();
	proc_init();

	if(!(mth->flags & MULTIBOOT_FLAG_MEM))
		panic("don't know how to detect memory!");
	struct mboot_module *m = mb->mods_count == 0 ? NULL : mm_ptov(mb->mods_addr);
	x86_64_top_mem = mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET;
	x86_64_bot_mem = (m && m->end > PHYS((uintptr_t)&kernel_end)) ? m->end : PHYS((uintptr_t)&kernel_end);

	kernel_early_init();
	_init();
	kernel_init();
}

void x86_64_cpu_secondary_entry(struct processor *proc)
{
	idt_init_secondary();
	proc_init();
	x86_64_lapic_init_percpu();
	assert(proc != NULL);
	processor_secondary_entry(proc);
}


void x86_64_write_gdt_entry(struct x86_64_gdt_entry *entry, uint32_t base, uint32_t limit,
		uint8_t access, uint8_t gran)
{
	entry->base_low = base & 0xFFFF;
	entry->base_middle = (base >> 16) & 0xFF;
	entry->base_high = (base >> 24) & 0xFF;
	entry->limit_low = limit & 0xFFFF;
	entry->granularity = ((limit >> 16) & 0x0F) | ((gran & 0x0F) << 4);
	entry->access = access;
}

void x86_64_tss_init(struct processor *proc)
{
	struct x86_64_tss *tss = &proc->arch.tss;
	memset(tss, 0, sizeof(*tss));
	tss->ss0 = 0x10;
	tss->esp0 = 0;
	tss->cs = 0x0b;
	tss->ss = tss->ds = tss->es = 0x13;
	x86_64_write_gdt_entry(&proc->arch.gdt[5], (uint32_t)(uintptr_t)tss, sizeof(*tss), 0xE9, 0);
	x86_64_write_gdt_entry(&proc->arch.gdt[6], ((uintptr_t)tss >> 48) & 0xFFFF, ((uintptr_t)tss >> 32) & 0xFFFF, 0, 0);
	asm volatile("movw $0x2B, %%ax; ltr %%ax" ::: "rax", "memory");
}

void x86_64_gdt_init(struct processor *proc)
{
	memset(&proc->arch.gdt, 0, sizeof(proc->arch.gdt));
	x86_64_write_gdt_entry(&proc->arch.gdt[0], 0, 0, 0, 0);
	x86_64_write_gdt_entry(&proc->arch.gdt[1], 0, 0xFFFFF, 0x9A, 0xA); /* C64 K */
	x86_64_write_gdt_entry(&proc->arch.gdt[2], 0, 0xFFFFF, 0x92, 0xA); /* D64 K */
	x86_64_write_gdt_entry(&proc->arch.gdt[3], 0, 0xFFFFF, 0xF2, 0xA); /* D64 U */
	x86_64_write_gdt_entry(&proc->arch.gdt[4], 0, 0xFFFFF, 0xFA, 0xA); /* C64 U */
	proc->arch.gdtptr.limit = sizeof(struct x86_64_gdt_entry) * 8 - 1;
	proc->arch.gdtptr.base = (uintptr_t)&proc->arch.gdt;
	asm volatile("lgdt (%0)" :: "r"(&proc->arch.gdtptr));
}

extern int initial_boot_stack;
extern void x86_64_syscall_entry_from_userspace();
extern void kernel_main(struct processor *proc);

void x86_64_processor_post_vm_init(struct processor *proc)
{
	
	/* save GS kernel base (saved to user, because we swapgs on sysret) */
	uint64_t gs = (uint64_t)&proc->arch;
	x86_64_wrmsr(X86_MSR_GS_BASE, gs & 0xFFFFFFFF, gs >> 32);

	/* okay, now set up the registers for fast syscall.
	 * This means storing x86_64_syscall_entry to LSTAR,
	 * the EFLAGS mask to SFMASK, and the CS kernel segment
	 * to STAR. */

	
	/* STAR: bits 32-47 are kernel CS, 48-63 are user CS. */
	uint32_t lo = 0, hi;
	hi = (0x10 << 16) | 0x08;
	x86_64_wrmsr(X86_MSR_STAR, lo, hi);

	/* LSTAR: contains kernel entry point for syscall */
	lo = (uintptr_t)(&x86_64_syscall_entry_from_userspace) & 0xFFFFFFFF;
	hi = ((uintptr_t)(&x86_64_syscall_entry_from_userspace) >> 32) & 0xFFFFFFFF;
	x86_64_wrmsr(X86_MSR_LSTAR, lo, hi);

	/* SFMASK contains mask for eflags. Each bit set in SFMASK will
	 * be cleared in eflags on syscall */
	/*      TF         IF          DF        IOPL0       IOPL1         NT         AC */
	lo = (1 << 8) | (1 << 9) | (1 << 10) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 18);
	hi = 0;
	x86_64_wrmsr(X86_MSR_SFMASK, lo, hi);
	proc->arch.curr = NULL;

	kernel_main(proc);
}

void arch_processor_init(struct processor *proc)
{
	x86_64_gdt_init(proc);
	x86_64_tss_init(proc);
	if(proc->flags & PROCESSOR_BSP) {
		proc->arch.kernel_stack = &initial_boot_stack;
	}

	x86_64_processor_post_vm_init(proc);
	x86_64_start_vmx(proc);

	/* TODO: this code is never reached, but is left here for reference. Delete it soon. */

	/* save GS kernel base (saved to user, because we swapgs on sysret) */
	uint64_t gs = (uint64_t)&proc->arch;
	x86_64_wrmsr(X86_MSR_GS_BASE, gs & 0xFFFFFFFF, gs >> 32);

	/* okay, now set up the registers for fast syscall.
	 * This means storing x86_64_syscall_entry to LSTAR,
	 * the EFLAGS mask to SFMASK, and the CS kernel segment
	 * to STAR. */

	
	/* STAR: bits 32-47 are kernel CS, 48-63 are user CS. */
	uint32_t lo = 0, hi;
	hi = (0x10 << 16) | 0x08;
	x86_64_wrmsr(X86_MSR_STAR, lo, hi);

	/* LSTAR: contains kernel entry point for syscall */
	lo = (uintptr_t)(&x86_64_syscall_entry_from_userspace) & 0xFFFFFFFF;
	hi = ((uintptr_t)(&x86_64_syscall_entry_from_userspace) >> 32) & 0xFFFFFFFF;
	x86_64_wrmsr(X86_MSR_LSTAR, lo, hi);

	/* SFMASK contains mask for eflags. Each bit set in SFMASK will
	 * be cleared in eflags on syscall */
	/*      TF         IF          DF        IOPL0       IOPL1         NT         AC */
	lo = (1 << 8) | (1 << 9) | (1 << 10) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 18);
	hi = 0;
	x86_64_wrmsr(X86_MSR_SFMASK, lo, hi);
	proc->arch.curr = NULL;
}

void arch_thread_init(struct thread *thread, void *entry, void *arg, void *stack)
{
	memset(&thread->arch.syscall, 0, sizeof(thread->arch.syscall));
	thread->arch.syscall.rcx = (uint64_t)entry;
	thread->arch.syscall.rsp = (uint64_t)stack;
	thread->arch.syscall.rdi = (uint64_t)arg;
	thread->arch.was_syscall = 1;
	thread->arch.usedfpu = false;
	thread->arch.fs = thread->arch.gs = 0;
}

