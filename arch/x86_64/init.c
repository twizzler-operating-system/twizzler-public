#include <arch/x86_64-acpi.h>
#include <arch/x86_64-msr.h>
#include <arch/x86_64-vmx.h>
#include <debug.h>
#include <init.h>
#include <kc.h>
#include <machine/memory.h>
#include <machine/pc-multiboot.h>
#include <machine/pc-multiboot2.h>
#include <processor.h>
#include <string.h>
#include <tmpmap.h>

/* TODO (major): clean up this file */

void serial_init();

extern void _init();
extern int initial_boot_stack;
uint64_t x86_64_top_mem;
uint64_t x86_64_bot_mem;
extern void idt_init(void);
extern void idt_init_secondary(void);

static struct processor _dummy_proc;

static size_t xsave_region_size = 512;

static void proc_init(void)
{
	uint64_t cr0, cr4;
	asm volatile("finit");
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	// cr0 |= (1 << 2);
	cr0 |= (1 << 5);
	cr0 |= (1 << 1);
	cr0 |= (1 << 16);
	cr0 &= ~(1 << 30); // make sure caching is on
	cr0 &= ~(1 << 29); // make sure caching is on
	cr0 &= ~(1 << 2);  // make sure caching is on
	asm volatile("mov %0, %%cr0" ::"r"(cr0));

	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1 << 10); // enable fast fxsave etc, sse
	cr4 |= (1 << 16); // rdgsbase
	cr4 |= (1 << 18);
	cr4 |= (1 << 9);
	cr4 |= (1 << 6); // enable MCE
	/* clear any global mappings in the TLB */
	cr4 &= ~(1 << 7);
	asm volatile("mov %0, %%cr4" ::"r"(cr4));
	/* enable global mappings */
	cr4 |= (1 << 7); // enable page global
	asm volatile("mov %0, %%cr4" ::"r"(cr4));
	// printk("cr4: %lx, cr0: %lx\n", cr4, cr0);

	asm volatile("xor %%rcx, %%rcx; xgetbv; or $7, %%rax; xsetbv;" ::: "rax", "rcx", "rdx");
	/* enable fast syscall extension */
	uint32_t lo, hi;
	x86_64_rdmsr(X86_MSR_EFER, &lo, &hi);
	lo |= X86_MSR_EFER_SYSCALL | X86_MSR_EFER_NX;
	x86_64_wrmsr(X86_MSR_EFER, lo, hi);

	x86_64_rdmsr(0x1a0, &lo, &hi);

	/* TODO (minor): verify that this setup is "reasonable" */
	x86_64_rdmsr(X86_MSR_MTRRCAP, &lo, &hi);
	int mtrrcnt = lo & 0xFF;
	x86_64_rdmsr(X86_MSR_MTRR_DEF_TYPE, &lo, &hi);
	for(int i = 0; i < mtrrcnt; i++) {
		x86_64_rdmsr(X86_MSR_MTRR_PHYSBASE(i), &lo, &hi);
		//	printk("mttr base %d: %x %x\n", i, hi, lo);
		x86_64_rdmsr(X86_MSR_MTRR_PHYSMASK(i), &lo, &hi);
		//	printk("mttr mask %d: %x %x\n", i, hi, lo);
	}

	// x86_64_rdmsr(0x277 /* PAT */, &lo, &hi);
	// uint64_t pat = ((long)hi << 32) | (long)lo;
	// printk("PAT: %lx\n", pat);

	/* in case we need to field an interrupt before we properly setup gs */
	uint64_t gs = (uint64_t)&_dummy_proc.arch;
	x86_64_wrmsr(X86_MSR_GS_BASE, gs & 0xFFFFFFFF, gs >> 32);

	// for(int i = 2; i <= 2; i++) {
	//	size_t sz = x86_64_cpuid(0xd, i, 0);
	//	size_t base = x86_64_cpuid(0xd, i, 1);
	//	if(base + sz > xsave_region_size)
	//		xsave_region_size = base + sz;
	//}
	xsave_region_size = 1024; // TODO
	align_up(xsave_region_size, 64);

	uint16_t fcw;
	asm volatile("fstcw %0" : "=m"(fcw));
	fcw |= 0x33f; // double-prec, mask all
	asm volatile("fldcw %0" : "=m"(fcw));
	uint32_t mxcsr;
	asm volatile("stmxcsr %0" : "=m"(mxcsr));
	mxcsr |= 0x1f80; // mask all
	asm volatile("ldmxcsr %0" : "=m"(mxcsr));
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
#define PHYS(x) ((x)-PHYS_ADDR_DELTA)
extern int kernel_end;
extern int kernel_start;
#include <object.h>
#include <page.h>

extern objid_t kc_init_id;

void load_object_data(struct object *obj, char *tardata, size_t tarlen)
{
	struct ustar_header *h = (struct ustar_header *)tardata;
	while((char *)h < tardata + tarlen) {
		char *name = h->name;
		char *data = (char *)h + 512;
		if(!*name)
			break;
		if(strncmp(h->magic, "ustar", 5)) {
			printk("Malformed object data\n");
			break;
		}

		size_t len = strtol(h->size, NULL, 8);
		size_t reclen = (len + 511) & ~511;
		size_t nl = strlen(name);

		size_t idx = 1;
		if(!strncmp(name, "data", nl) && nl == 4) {
		} else if(!strncmp(name, "meta", nl) && nl == 4) {
			idx = (OBJ_MAXSIZE - (len)) / mm_page_size(0);
		} else {
			printk("Malformed object data\n");
		}

		//	obj_write_data(obj, 0, len, data);

		for(size_t s = 0; s < len; s += mm_page_size(0), idx++) {
			struct page *page = page_alloc(PAGE_TYPE_VOLATILE, 0, 0);

			size_t thislen = 0x1000;
			if((len - s) < thislen)
				thislen = len - s;
			void *addr = tmpmap_map_page(page);
			memset(addr, 0, mm_page_size(page->level));
			memcpy(addr, data + s, thislen);
			if((obj->id & 0xfffffffffffffffful) == 0xDA5366BF8BB01253ul) {
				printk("cache page: %p %lx: %lx\n", page, page->addr, mm_vtop(addr));
			}
			tmpmap_unmap_page(addr);
			obj_cache_page(obj, idx * mm_page_size(0), page);
		}

		h = (struct ustar_header *)((char *)h + 512 + reclen);
	}
}

static size_t _load_initrd(char *start, size_t modlen)
{
	printk(":: load_initrd: %p\n", start);
	struct ustar_header *h = (void *)start;

	size_t count = 0;
	while((char *)h < start + modlen) {
		char *name = h->name;
		if(!*name)
			break;
		if(strncmp(h->magic, "ustar", 5))
			break;
		char *data = (char *)h + 512;
		size_t len = strtol(h->size, NULL, 8);
		size_t reclen = (len + 511) & ~511;

		switch(h->typeflag[0]) {
			size_t nl;
			case '0':
			case '7':
				nl = strlen(name);
				// printk("Loading object: %s\e[K\r", name);
				if(!strncmp(name, "kc", 2) && nl == 2) {
					kc_parse(data, len);
				} else {
					if(nl < 33) {
						printk("Malformed object name: %s\n", name);
						break;
					} else if(nl > 33 && nl != 38) {
						printk("Malformed object name: %s\n", name);
						break;
					} else if(nl == 38 && strncmp(name + 33, ".meta", 5)) {
						printk("Malformed object name: %s\n", name);
						break;
					}
					objid_t id;
					if(!objid_parse(name, &id)) {
						printk("Malformed object name: %s\n", name);
						break;
					}

					struct object *obj = obj_lookup(id);
					if(obj == NULL) {
						obj = obj_create(id, KSO_NONE);
					}
					load_object_data(obj, data, len);
					count++;
				}
				break;
			default:
				// printk("unsupported ustar type %c for %s\n", h->typeflag[0], name);
				break;
		}

		h = (struct ustar_header *)((char *)h + 512 + reclen);
	}
	return count;
}

void x86_64_reclaim_initrd_region(void);
static void *mod_start;
static size_t mod_len;
static void x86_64_initrd2(void *u __unused)
{
	size_t c = _load_initrd(mod_start, mod_len);
	printk("[initrd] loaded %ld objects\e[0K\n", c);
	x86_64_reclaim_initrd_region();
}
POST_INIT(x86_64_initrd2, NULL);

void kernel_early_init(void);
void kernel_init(void);
void x86_64_lapic_init_percpu(void);
void x86_64_memory_record(uintptr_t addr,
  size_t len,
  enum memory_type type,
  enum memory_subtype st);

void x86_64_register_kernel_region(uintptr_t addr, size_t len);
void x86_64_register_initrd_region(uintptr_t addr, size_t len);

static enum memory_type memory_type_map(unsigned int mtb_type)
{
	static enum memory_type _types[] = {
		[MULTIBOOT_MEMORY_AVAILABLE] = MEMORY_AVAILABLE,
		[MULTIBOOT_MEMORY_PERSISTENT] = MEMORY_AVAILABLE,
		[MULTIBOOT_MEMORY_CODE] = MEMORY_CODE,
		[MULTIBOOT_MEMORY_RESERVED] = MEMORY_RESERVED,
		[MULTIBOOT_MEMORY_NVS] = MEMORY_RESERVED,
		[MULTIBOOT_MEMORY_ACPI_RECLAIMABLE] = MEMORY_RECLAIMABLE,
		[MULTIBOOT_MEMORY_BADRAM] = MEMORY_BAD,
	};
	return mtb_type >= sizeof(_types) / sizeof(_types[0]) ? MEMORY_UNKNOWN : _types[mtb_type];
}

static enum memory_subtype memory_subtype_map(unsigned int mtb_type)
{
	static enum memory_type _subtypes[] = {
		[MULTIBOOT_MEMORY_AVAILABLE] = MEMORY_AVAILABLE_VOLATILE,
		[MULTIBOOT_MEMORY_PERSISTENT] = MEMORY_AVAILABLE_PERSISTENT,
	};
	return mtb_type >= sizeof(_subtypes) / sizeof(_subtypes[0]) ? MEMORY_SUBTYPE_NONE
	                                                            : _subtypes[mtb_type];
}

void x86_64_init(uint32_t magic, struct multiboot *mth)
{
	idt_init();
	serial_init();
	proc_init();

	// void (*initrd_hook)(void *) = NULL;
	if(magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
		struct multiboot_info *info = (void *)mth;
		struct multiboot_tag *tag;
		struct multiboot_tag_mmap *mmap_tag = NULL;
		struct multiboot_tag_module *module_tag = NULL;
		for(char *t = info->tags; t < info->tags + (info->total_size - sizeof(*info));
		    t += align_up(tag->size, 8)) {
			tag = (void *)t;
			switch(tag->type) {
				struct multiboot_tag_old_acpi *acpi_old_tag;
				struct multiboot_tag_new_acpi *acpi_new_tag;
				case MULTIBOOT_TAG_TYPE_END:
					/* force loop to exit */
					t = info->tags + info->total_size;
					break;
				case MULTIBOOT_TAG_TYPE_MMAP:
					mmap_tag = (void *)tag;
					break;
				case MULTIBOOT_TAG_TYPE_MODULE:
					module_tag = (void *)tag;
					break;
				case MULTIBOOT_TAG_TYPE_ACPI_OLD:
					acpi_old_tag = (void *)tag;
					acpi_set_rsdp(acpi_old_tag->rsdp, tag->size - sizeof(*acpi_old_tag));
					break;
				case MULTIBOOT_TAG_TYPE_ACPI_NEW:
					acpi_new_tag = (void *)tag;
					acpi_set_rsdp(acpi_new_tag->rsdp, tag->size - sizeof(*acpi_new_tag));
					break;
			}
		}
		if(!mmap_tag)
			panic("multiboot2 information structure did not contain memory maps.");

		/* do the memory map processing later, since we'll need to not report regions taken up by
		 * the kernel, or by modules */
		size_t nr_entries = (mmap_tag->size - sizeof(*mmap_tag)) / mmap_tag->entry_size;
		uintptr_t ks_addr = align_down(PHYS((uintptr_t)&kernel_start), 0x1000);
		uintptr_t ke_addr = align_up(PHYS((uintptr_t)&kernel_end), 0x1000);
		uintptr_t ms = module_tag ? align_down(module_tag->mod_start, 0x1000) : 0;
		uintptr_t me = module_tag ? align_up(module_tag->mod_end, 0x1000) : 0;

		x86_64_register_kernel_region(ks_addr, ke_addr - ks_addr);
		x86_64_register_initrd_region(ms, me - ms);

		for(size_t i = 0; i < nr_entries; i++) {
			struct multiboot_mmap_entry *entry =
			  (void *)((char *)mmap_tag->entries + i * mmap_tag->entry_size);

			size_t processed_len;
			for(size_t off = 0; off < entry->len; off += processed_len) {
				processed_len = entry->len - off;

				if(module_tag && ms == entry->addr + off) {
					/* skip to the end of the module */
					processed_len = me - ms;
					continue;
				} else if(module_tag && ms > entry->addr + off && ms < entry->addr + entry->len) {
					/* start of module is within this region, but there's some free memory here. */
					processed_len = ms - (off + entry->addr);
				} else if(module_tag && me > entry->addr + off && me < entry->addr + entry->len) {
					/* the end of the module is in this region; skip to the end. */
					processed_len = me - (off + entry->addr);
					continue;
				}

				if(ks_addr == entry->addr + off) {
					/* skip to the end of the module */
					processed_len = ke_addr - ks_addr;
					continue;
				} else if(ks_addr > entry->addr + off && ks_addr < entry->addr + processed_len) {
					/* start of module is within this region, but there's some free memory here. */
					processed_len = ks_addr - (off + entry->addr);
				} else if(ke_addr > entry->addr + off && ke_addr < entry->addr + processed_len) {
					/* the end of the module is in this region; skip to the end. */
					processed_len = ke_addr - (off + entry->addr);
					continue;
				}

				assert(processed_len > 0);

				if(off + processed_len > entry->len) {
					processed_len = entry->len - off;
				}

				x86_64_memory_record(entry->addr + off,
				  processed_len,
				  memory_type_map(entry->type),
				  memory_subtype_map(entry->type));
			}
		}
		if(module_tag) {
			mod_start = mm_ptov(module_tag->mod_start);
			mod_len = module_tag->mod_end - module_tag->mod_start;
		}
	} else if(magic == 0x2BADB002) {
		panic("Twizzler no longer supports multiboot 1; please upgrade to multiboot 2");
	} else {
		panic("unknown bootloader type!");
	}

	kernel_early_init();
	x86_64_lapic_init_percpu();

	/* need to wait on this until here because this requires memory allocation */
	current_processor->arch.kernel_stack = &initial_boot_stack;
	current_processor->flags |= PROCESSOR_BSP;
	x86_64_start_vmx(current_processor);
	panic("got here");
	//	kernel_init();
}

void x86_64_secondary_vm_init(void);

void x86_64_cpu_secondary_entry(struct processor *proc)
{
	idt_init_secondary();
	proc_init();
	x86_64_lapic_init_percpu();
	assert(proc != NULL);
	x86_64_start_vmx(proc);
}

void x86_64_write_gdt_entry(struct x86_64_gdt_entry *entry,
  uint32_t base,
  uint32_t limit,
  uint8_t access,
  uint8_t gran)
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
	tss->ist[X86_DOUBLE_FAULT_IST_IDX] = (uintptr_t)proc->arch.kernel_stack + KERNEL_STACK_SIZE;
	x86_64_write_gdt_entry(&proc->arch.gdt[5], (uint32_t)(uintptr_t)tss, sizeof(*tss), 0xE9, 0);
	x86_64_write_gdt_entry(
	  &proc->arch.gdt[6], ((uintptr_t)tss >> 48) & 0xFFFF, ((uintptr_t)tss >> 32) & 0xFFFF, 0, 0);
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
	asm volatile("lgdt (%0)" ::"r"(&proc->arch.gdtptr));
}

extern void x86_64_syscall_entry_from_userspace();
extern void kernel_main(struct processor *proc);

void x86_64_vm_kernel_context_init(void);
void x86_64_processor_post_vm_init(struct processor *proc)
{
	x86_64_vm_kernel_context_init();
	x86_64_gdt_init(proc);
	x86_64_tss_init(proc);
	/* save GS kernel base (saved to user, because we swapgs on sysret) */
	uint64_t gs = (uint64_t)&proc->arch;
	x86_64_wrmsr(X86_MSR_GS_BASE, gs & 0xFFFFFFFF, gs >> 32);

	/* okay, now set up the registers for fast syscall, which we can do after we
	 * enter vmx-non-root because only userspace needs these.
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

	if(proc->flags & PROCESSOR_BSP)
		kernel_init();
	proc->arch.kernel_stack = mm_memory_alloc(KERNEL_STACK_SIZE, PM_TYPE_DRAM, true);
	printk(":: %p\n", proc->arch.kernel_stack);
	asm volatile("mov %%rax, %%rsp; call processor_perproc_init;" ::"a"(
	               proc->arch.kernel_stack + KERNEL_STACK_SIZE),
	             "D"(proc)
	             : "memory");
	panic("returned here");
	// processor_perproc_init(proc);
}

void arch_processor_early_init(struct processor *proc)
{
	/* TODO: some of this can be free'd later. Implement a system to free early_memory. Add it to a
	 * list, and then after init call some function that converts these to usable pages? */
	proc->arch.hyper_stack = mm_virtual_early_alloc();
	proc->arch.kernel_stack = mm_virtual_early_alloc();
	proc->arch.veinfo = mm_virtual_early_alloc();
	proc->arch.vmcs = mm_physical_early_alloc();
	proc->arch.vmxon_region = mm_physical_early_alloc();
}

void arch_processor_init(struct processor *proc)
{
	/* set GS before we enter the vmx-non-root. Host and guest need to know
	 * what GS should be. */
	uint64_t gs = (uint64_t)&proc->arch;
	x86_64_wrmsr(X86_MSR_GS_BASE, gs & 0xFFFFFFFF, gs >> 32);
	// x86_64_start_vmx(proc);
}

void arch_thread_init(struct thread *thread,
  void *entry,
  void *arg,
  void *stack,
  size_t stacksz,
  void *tls,
  size_t thrd_ctrl_slot)
{
	memset(&thread->arch.syscall, 0, sizeof(thread->arch.syscall));
	thread->arch.syscall.rcx = (uint64_t)entry;
	thread->arch.syscall.rsp = (uint64_t)stack + stacksz - 8;
	thread->arch.syscall.rdi = (uint64_t)arg;
	thread->arch.syscall.rsi = ID_LO(thread->thrid);
	thread->arch.syscall.rdx = ID_HI(thread->thrid);
	thread->arch.was_syscall = 1;
	thread->arch.fs = (long)tls; /* TODO: only set one of these */
	thread->arch.gs = (long)thrd_ctrl_slot * mm_page_size(MAX_PGLEVEL);
	if(xsave_region_size > 0x1000)
		panic("NI - HUGE xsave region");
	/* TODO: make sure to free this */
	thread->arch.xsave_region = (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
}
