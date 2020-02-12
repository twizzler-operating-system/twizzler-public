#include <arena.h>
#include <clksrc.h>
#include <debug.h>
#include <device.h>
#include <init.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <processor.h>
#include <secctx.h>
#include <thread.h>
#include <time.h>
#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/driver/system.h>

#include <twz/_objid.h>
#include <twz/_slots.h>
#include <twz/_thrd.h>

struct object *get_system_object(void)
{
	static struct object *system_bus;
	static struct spinlock lock = SPINLOCK_INIT;
	static _Atomic bool init = false;

	if(!init) {
		spinlock_acquire_save(&lock);
		if(!init) {
			system_bus = bus_register(DEVICE_BT_SYSTEM, 0, sizeof(struct system_header));
			kso_setname(system_bus, "System");
			kso_root_attach(system_bus, 0, KSO_DEVBUS);
			init = true;
		}
		spinlock_release_restore(&lock);
	}
	return system_bus;
}

static struct arena post_init_call_arena;
static struct init_call *post_init_call_head = NULL;

void post_init_call_register(bool ac, void (*fn)(void *), void *data)
{
	if(post_init_call_head == NULL) {
		arena_create(&post_init_call_arena);
	}

	struct init_call *ic = arena_allocate(&post_init_call_arena, sizeof(struct init_call));
	ic->fn = fn;
	ic->data = data;
	ic->allcpus = ac;
	ic->next = post_init_call_head;
	post_init_call_head = ic;
}

static void post_init_calls_execute(bool secondary)
{
	for(struct init_call *call = post_init_call_head; call != NULL; call = call->next) {
		if(!secondary || call->allcpus) {
			call->fn(call->data);
		}
	}
}

/* functions called from here expect virtual memory to be set up. However, functions
 * called from here cannot rely on global contructors having run, as those are allowed
 * to use memory management routines, so they are run after this. Furthermore,
 * they cannot use per-cpu data.
 */
void kernel_early_init(void)
{
	mm_init();
	processor_percpu_regions_init();
	processor_early_init();
}

/* at this point, memory management, interrupt routines, global constructors, and shared
 * kernel state between nodes have been initialized. Now initialize all application processors
 * and per-node threading.
 */

extern void _init(void);
void kernel_init(void)
{
	page_init_bootstrap();
	mm_init_phase_2();
	_init();
	processor_init_secondaries();
}

#if 0
static void bench(void)
{
	printk("Starting benchmark\n");
	arch_interrupt_set(true);
	return;
	int c = 0;
	for(c = 0; c < 5; c++) {
		// uint64_t sr = rdtsc();
		// uint64_t start = clksrc_get_nanoseconds();
		// uint64_t end = clksrc_get_nanoseconds();
		// uint64_t er = rdtsc();
		// printk(":: %ld %ld\n", end - start, er - sr);
		// printk(":: %ld\n", er - sr);

#if 0
		uint64_t start = clksrc_get_nanoseconds();
		volatile int i;
		uint64_t c = 0;
		int64_t max = 1000000000;
		for(i = 0; i < max; i++) {
			volatile int x = i ^ (i - 1);
			//	uint64_t x = rdtsc();
			// clksrc_get_nanoseconds();
			//	uint64_t y = rdtsc();
			//	c += (y - x);
		}
		uint64_t end = clksrc_get_nanoseconds();
		printk("Done: %ld (%ld)\n", end - start, (end - start) / i);
		// printk("RD: %ld (%ld)\n", c, c / i);
		start = clksrc_get_nanoseconds();
		for(i = 0; i < max; i++) {
			us1[i % 0x1000] = i & 0xff;
		}
		end = clksrc_get_nanoseconds();
		printk("MEMD: %ld (%ld)\n", end - start, (end - start) / i);
#else
		while(true) {
			uint64_t t = clksrc_get_nanoseconds();
			if(((t / 1000000) % 1000) == 0)
				printk("ONE SECOND %ld\n", t);
		}
		uint64_t start = clksrc_get_nanoseconds();
		// for(long i=0;i<800000000l;i++);
		for(long i = 0; i < 800000000l; i++)
			;
		uint64_t end = clksrc_get_nanoseconds();
		printk("Done: %ld\n", end - start);
		if(c++ == 10)
			panic("reset");
#endif
	}
	for(;;)
		;
}
#endif

static _Atomic unsigned int kernel_main_barrier = 0;

#include <kc.h>
#include <object.h>

struct elf64_header {
	uint8_t e_ident[16];
	uint16_t e_type;
	uint16_t e_machine;
	uint32_t e_version;
	uint64_t e_entry;
	uint64_t e_phoff;
	uint64_t e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize;
	uint16_t e_phentsize;
	uint16_t e_phnum;
	uint16_t e_shentsize;
	uint16_t e_shnum;
	uint16_t e_shstrndx;
};

#include <kalloc.h>
/* TODO: a real allocator */
static struct arena kalloc_arena;

void *krealloc(void *p, size_t sz)
{
	size_t *s = (void *)((char *)p - sizeof(size_t));
	if(*s >= sz)
		return p;

	void *n = kalloc(sz);
	memcpy(n, p, *s);
	kfree(p);
	return n;
}

void *krecalloc(void *p, size_t num, size_t sz)
{
	return krealloc(p, num * sz);
}

void *kalloc(size_t sz)
{
	static _Atomic bool _init = false;
	static struct spinlock wait = SPINLOCK_INIT;
	if(!_init) {
		spinlock_acquire_save(&wait);
		if(!_init) {
			arena_create(&kalloc_arena);
			_init = true;
		}
		spinlock_release_restore(&wait);
	}
	sz += sizeof(size_t);
	void *p = arena_allocate(&kalloc_arena, sz);
	*(size_t *)p = sz;
	p = (char *)p + sizeof(size_t);
	// printk("kalloc: %ld -> %p\n", sz, p);
	return p;
}

void *kcalloc(size_t num, size_t sz)
{
	/* TODO: overflow check */
	void *p = kalloc(num * sz);
	if(p)
		memset(p, 0, num * sz);
	return p;
}

void kfree(void *p)
{
	(void)p;
	// printk("[ni] kfree (%p)\n", p);
}
static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#include <syscall.h>
void kernel_main(struct processor *proc)
{
	mm_print_stats();
	if(proc->flags & PROCESSOR_BSP) {
		/* create the root object. TODO: load an old one? */
		struct object *root = obj_create(KSO_ROOT_ID, KSO_ROOT);
		struct metainfo mi = {
			.p_flags = MIP_DFL_READ,
			.flags = 0,
			.milen = sizeof(mi) + 128,
			.kuid = 0,
			.nonce = 0,
			.magic = MI_MAGIC,
		};
		root->idversafe = true;
		root->idvercache = true;

		obj_write_data(
		  root, OBJ_MAXSIZE - (OBJ_NULLPAGE_SIZE + OBJ_METAPAGE_SIZE), sizeof(mi), &mi);
		struct object *so = get_system_object();
		struct system_header *hdr = bus_get_busspecific(so);
		hdr->pagesz = mm_page_size(0);
		device_release_headers(so);
	}
	post_init_calls_execute(!(proc->flags & PROCESSOR_BSP));

	printk("Waiting at kernel_main_barrier\n");
	processor_barrier(&kernel_main_barrier);
	printk("POST BARRIER\n");

	if(proc->flags & PROCESSOR_BSP) {
		arena_destroy(&post_init_call_arena);
		post_init_call_head = NULL;

		// bench();
		// if(kc_bsv_id == 0) {
		//	panic("No bsv specified");
		//}
		if(kc_init_id == 0) {
			panic("No init specified");
		}

		struct object *initobj = obj_lookup(kc_init_id);
		if(!initobj) {
			panic("Cannot load init object");
		}

		struct elf64_header elf;
		obj_read_data(initobj, 0, sizeof(elf), &elf);
		if(memcmp("\x7F"
		          "ELF",
		     elf.e_ident,
		     4)) {
			printk("---> %x %x %x\n", elf.e_ident[0], elf.e_ident[1], elf.e_ident[2]);
			panic("Init is not an ELF file");
		}

		obj_put(initobj);

#define US_STACK_SIZE 0x200000 - 0x1000
		char *stck_obj = (void *)(0x400040000000ull);
		char *thrd_obj = (void *)(0x400000000000ull);
		size_t off = US_STACK_SIZE - 0x100, tmp = 0;

		//	printk("stck slot = %ld\n", (uintptr_t)stck_obj / mm_page_size(MAX_PGLEVEL));
		//	printk("thrd slot = %ld\n", (uintptr_t)thrd_obj / mm_page_size(MAX_PGLEVEL));

		char name_id[64];
		snprintf(name_id, 64, "BSNAME=" IDFMT, IDPR(kc_name_id));

		long vector[6] = {
			[0] = 1, /* argc */
			[1] = 0, /* argv[0] */
			[2] = 0, /* argv[1] == NULL */
			[3] = 0, /* envp[0] == NAME */
			[4] = 0, /* envp[1] == NULL */
			[5] = 0, /* envp[2] == NULL */
			         //[1] = (long)thrd_obj + off + sizeof(vector) + 0x1000,
		};

		objid_t bthrid;
		objid_t bstckid;
		objid_t bsvid;

		int r;
		r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &bthrid);
		if(r < 0)
			panic("failed to create initial objects: %d", r);
		r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &bstckid);
		if(r < 0)
			panic("failed to create initial objects: %d", r);
		r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &bsvid);
		if(r < 0)
			printk(
			  "Created bthrd = " IDFMT " and bstck = " IDFMT "\n", IDPR(bthrid), IDPR(bstckid));

		struct object *bthr = obj_lookup(bthrid);
		struct object *bstck = obj_lookup(bstckid);
		struct object *bv = obj_lookup(bsvid);
		assert(bthr && bstck && bv);

		struct viewentry v_t = {
			.id = bthrid,
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};
		struct viewentry v_s = {
			.id = bstckid,
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};

		struct viewentry v_v = {
			.id = bsvid,
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};

		kso_view_write(bv, TWZSLOT_CVIEW, &v_v);

		struct viewentry v_i = {
			.id = kc_init_id,
			.flags = VE_READ | VE_EXEC | VE_VALID,
		};

		kso_view_write(bv, 0, &v_i);

		bthr->flags |= OF_KERNELGEN;
		bstck->flags |= OF_KERNELGEN;

		char *init_argv0 = "___init";
		obj_write_data(bstck, off, strlen(init_argv0) + 1, init_argv0);
		vector[1] = (long)stck_obj + off + 0x1000;
		off += strlen(init_argv0) + 1;

		obj_write_data(bstck, off, strlen(name_id) + 1, name_id);
		vector[3] = (long)stck_obj + off + 0x1000;
		off += strlen(name_id) + 1;

		obj_write_data(bstck, off + tmp, sizeof(long), &vector[0]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[1]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[2]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[3]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[4]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[5]);
		tmp += sizeof(long);

		// obj_write_data(bthr, off + tmp, sizeof(char *) * 4, argv);

		obj_write_data(bthr,
		  sizeof(struct twzthread_repr)
		    + ((uintptr_t)thrd_obj / mm_page_size(MAX_PGLEVEL)) * sizeof(struct viewentry),
		  sizeof(struct viewentry),
		  &v_t);

		obj_write_data(bthr,
		  sizeof(struct twzthread_repr)
		    + ((uintptr_t)stck_obj / mm_page_size(MAX_PGLEVEL)) * sizeof(struct viewentry),
		  sizeof(struct viewentry),
		  &v_s);

		struct sys_thrd_spawn_args tsa = {
			.start_func = (void *)elf.e_entry,
			.stack_base = (void *)stck_obj + 0x1000,
			.stack_size = (US_STACK_SIZE - 0x100),
			.tls_base = stck_obj + 0x1000 + US_STACK_SIZE,
			.arg = stck_obj + off + 0x1000,
			.target_view = bsvid,
			.thrd_ctrl = (uintptr_t)thrd_obj / mm_page_size(MAX_PGLEVEL),
		};
#if 0
		printk("stackbase: %lx, stacktop: %lx\ntlsbase: %lx, arg: %lx\n",
				(long)tsa.stack_base, (long)tsa.stack_base + tsa.stack_size,
				(long)tsa.tls_base, (long)tsa.arg);
#endif
		r = syscall_thread_spawn(ID_LO(bthrid), ID_HI(bthrid), &tsa, 0);
		if(r < 0) {
			panic("thread_spawn: %d\n", r);
		}
	}
	if((proc->id == 0) && 0) {
		arch_interrupt_set(true);
		while(1) {
			long long a = rdtsc();
			for(volatile long i = 0; i < 1000000000l; i++) {
				asm volatile("" ::: "memory");
			}
			long long b = rdtsc();
			printk("K %d: %lld\n", proc->id, b - a);
		}
	}
#if 0
	printk("processor %d (%s) reached resume state %p\n",
	  proc->id,
	  proc->flags & PROCESSOR_BSP ? "bsp" : "aux",
	  proc);
#endif
	thread_schedule_resume_proc(proc);
}
