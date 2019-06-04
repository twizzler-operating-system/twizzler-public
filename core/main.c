#include <arena.h>
#include <clksrc.h>
#include <debug.h>
#include <init.h>
#include <memory.h>
#include <processor.h>
#include <secctx.h>
#include <thread.h>
#include <time.h>

#include <twz/_objid.h>

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
}

/* at this point, memory management, interrupt routines, global constructors, and shared
 * kernel state between nodes have been initialized. Now initialize all application processors
 * and per-node threading.
 */

void kernel_init(void)
{
	processor_init_secondaries();
	processor_perproc_init(NULL);
}

#if 0
static void bench(void)
{
	printk("Starting benchmark\n");
	arch_interrupt_set(true);

	int c = 0;
	for(c=0;c<5;c++)
	{
		//uint64_t sr = rdtsc();
		//uint64_t start = clksrc_get_nanoseconds();
		//uint64_t end = clksrc_get_nanoseconds();
		//uint64_t er = rdtsc();
		//printk(":: %ld %ld\n", end - start, er - sr);
		//printk(":: %ld\n", er - sr);

#if 1
		uint64_t start = clksrc_get_nanoseconds();
		volatile int i;
		uint64_t c = 0;
		int64_t max = 1000000000;
		for(i=0;i<max;i++) {
			volatile int x = i ^ (i-1);
		//	uint64_t x = rdtsc();
			//clksrc_get_nanoseconds();
		//	uint64_t y = rdtsc();
		//	c += (y - x);
		}
		uint64_t end = clksrc_get_nanoseconds();
		printk("Done: %ld (%ld)\n", end - start, (end - start) / i);
		//printk("RD: %ld (%ld)\n", c, c / i);
		start = clksrc_get_nanoseconds();
		for(i=0;i<max;i++) {
			us1[i % 0x1000] = i&0xff;
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
		//for(long i=0;i<800000000l;i++);
		for(long i=0;i<800000000l;i++);
		uint64_t end = clksrc_get_nanoseconds();
		printk("Done: %ld\n", end - start);
		if(c++ == 10)
			panic("reset");
#endif
	}
	for(;;);
}
#endif

static _Atomic unsigned int kernel_main_barrier = 0;

#include <kc.h>
#include <object.h>

objid_t objid_generate(void)
{
	static objid_t _id = 8;
	return _id++;
}

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

#include <syscall.h>
void kernel_main(struct processor *proc)
{
	post_init_calls_execute(!(proc->flags & PROCESSOR_BSP));

	printk("Waiting at kernel_main_barrier\n");
	processor_barrier(&kernel_main_barrier);

	if(proc->flags & PROCESSOR_BSP) {
		arena_destroy(&post_init_call_arena);
		post_init_call_head = NULL;
		// bench();
		if(kc_bsv_id == 0) {
			panic("No bsv specified");
		}

		if(kc_init_id == 0) {
			panic("No init specified");
		}

		objid_t kcid = 1;
		struct object *kcobj = obj_create(kcid, 0);
		obj_write_data(kcobj, 0, kc_len, kc_data);
		obj_put(kcobj);

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
		char *stck_obj = (void *)(0x400000000000ull);
		char *thrd_obj = (void *)(0x400010000000ull);
		size_t off = US_STACK_SIZE - 0x100, tmp = 0;

		char *argv[4] = {
			[0] = NULL,
			[1] = NULL,
			[2] = NULL,
			[3] = NULL,
		};
		long vector[4] = {
			[0] = 1, /* argc */
			[1] = 0,
			[2] = 0,
			[3] = 0,
			//[1] = (long)thrd_obj + off + sizeof(vector) + 0x1000,
		};

		objid_t bthrid = objid_generate();
		objid_t bstckid = objid_generate();
		printk("Created bthrd = " IDFMT " and bstck = " IDFMT "\n", IDPR(bthrid), IDPR(bstckid));
		struct object *bthr = obj_create(bthrid, KSO_THREAD);
		struct object *bstck = obj_create(bstckid, 0);
		struct object *bv = obj_lookup(kc_bsv_id);

		struct viewentry v_t = {
			.id = bthrid,
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};
		struct viewentry v_s = {
			.id = bstckid,
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};

		kso_view_write(bv, (uintptr_t)stck_obj / mm_page_size(MAX_PGLEVEL), &v_s);
		kso_view_write(bv, (uintptr_t)thrd_obj / mm_page_size(MAX_PGLEVEL), &v_t);

		bthr->flags |= OF_KERNELGEN;
		bstck->flags |= OF_KERNELGEN;

		obj_write_data(bstck, off + tmp, sizeof(long), &vector[0]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[1]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[2]);
		tmp += sizeof(long);
		obj_write_data(bstck, off + tmp, sizeof(long), &vector[3]);
		tmp += sizeof(long);

		obj_write_data(bthr, off + tmp, sizeof(char *) * 4, argv);

		struct sys_thrd_spawn_args tsa = {
			.start_func = (void *)elf.e_entry,
			.stack_base = (void *)stck_obj + 0x1000,
			.stack_size = (US_STACK_SIZE - 0x100),
			.tls_base = stck_obj + 0x1000 + US_STACK_SIZE,
			.arg = stck_obj + off + 0x1000,
			.target_view = kc_bsv_id,
		};

#if 0
		printk("stackbase: %lx, stacktop: %lx\ntlsbase: %lx, arg: %lx\n",
				(long)tsa.stack_base, (long)tsa.stack_base + tsa.stack_size,
				(long)tsa.tls_base, (long)tsa.arg);
#endif
		syscall_thread_spawn(ID_LO(bthrid), ID_HI(bthrid), &tsa, 0);
	}
	printk("processor %d reached resume state %p\n", proc->id, proc);
	thread_schedule_resume_proc(proc);
}
