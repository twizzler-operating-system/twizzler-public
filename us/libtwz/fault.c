#include <twzfault.h>
#include <stdatomic.h>
#include <twz.h>
#include <twzobj.h>
#include <twzview.h>
#include <twzsys.h>
#include <twzerr.h>
#include <twzname.h>
#include <twzslots.h>
#include <twzthread.h>

#include <debug.h>

#define _FAULT_DEBUG 0

#if !_FAULT_DEBUG
	#define debug_printf(...)
#endif

int twz_map_fot_entry(size_t slot, struct fotentry *fe)
{
	objid_t id;
	if(fe->flags & FE_NAME) {
		id = twz_name_resolve(NULL, fe->data, fe->nresolver);
		if(id == 0) {
			debug_printf("Name not resolved: %s\n", fe->data);
			return TE_FAILURE;
		}
	} else {
		id = fe->id;
	}

	int flags = ((fe->flags & FE_READ) ? VE_READ : 0)
		| ((fe->flags & FE_WRITE) ? VE_WRITE : 0)
		| ((fe->flags & FE_EXEC) ? VE_EXEC : 0);

	if(fe->flags & FE_DERIVE) {
		objid_t nid = 0;
		if(twz_object_new(NULL, &nid, id, 0 /* TODO: KUID, perms */,
					TWZ_ON_DFL_READ | TWZ_ON_DFL_WRITE) < 0) {
			debug_printf("Failed to make new derived object\n");
			return TE_FAILURE;
		}
		id = nid;
	}

	twz_view_set(NULL, slot, id, flags);
	debug_printf("Mapping slot: %ld :: " IDFMT "\n", slot, IDPR(id));
	
	return TE_SUCCESS;
}

int twz_handle_fault(uintptr_t addr, int cause, uintptr_t source __unused)
{
	uint64_t offset = addr % OBJ_SLOTSIZE;
	if(offset < OBJ_NULLPAGE_SIZE) {
		debug_printf("NULL ptr\n");
		return TE_FAILURE;
	}

	if(!(cause & FAULT_OBJECT_NOMAP)) {
		debug_printf("PERM err\n");
		return TE_FAILURE;
	}

	if(cause & FAULT_OBJECT_EXIST) {
		debug_printf("object no exist\n");
		return TE_FAILURE;
	}

	uint32_t obj0flags;
	objid_t obj0id;
	twz_view_get(NULL, 0, &obj0id, &obj0flags);
	if(!(obj0flags & VE_VALID) || obj0id == 0) {
		debug_printf("No obj0\n");
		return TE_FAILURE;
	}

	struct metainfo *mi = twz_slot_to_meta(0);
	if(mi->magic != MI_MAGIC) {
		debug_printf("No metainfo in object 0\n");
		return TE_FAILURE;
	}

	if(!(mi->flags & MIF_FOT)) {
		debug_printf("Object 0 has no FOT\n");
		return TE_FAILURE;
	}

	size_t slot = (addr / OBJ_SLOTSIZE) - 1;
	if(slot >= TWZSLOT_ALLOC_START) {
		debug_printf("Fault in allocatable object space (%lx)\n", slot+1);
		return TE_FAILURE;
	}
	if(slot >= 94) {
		debug_printf("TODO: large FOTs (%lx)\n", slot);
		return TE_FAILURE;
	}

	struct object obj0 = TWZ_OBJECT_INIT(0);
	struct fotentry *fot = twz_object_fot(&obj0, false);
	if(!fot) {
		debug_printf("obj 0 has no FOT\n");
	}

	if(fot[slot].id == 0) {
		debug_printf("Invalid FOT entry\n");
		return TE_FAILURE;
	}

	debug_printf("Slot: %ld - %s\n", slot+1, fot[slot].data);

	return twz_map_fot_entry(slot+1, &fot[slot]);
}

static int __fault_obj_default(int f __unused, void *_info)
{
	struct fault_object_info *info = _info;
	debug_printf("FAULT :: %d %lx %lx %lx", fault, info->ip, info->addr, info->flags);
	return twz_handle_fault(info->addr, info->flags, info->ip);
}

static struct {
	void (*fn)(int, void *);
} _fault_table[NUM_FAULTS] = { 0 };

static __attribute__((used)) void __twz_fault_entry_c(int fault, void *_info)
{
	if(fault == FAULT_OBJECT) {
		if(__fault_obj_default(fault, _info) == TE_FAILURE && !_fault_table[fault].fn) {
			twz_thread_exit();
		}
	}
	if((fault >= NUM_FAULTS || !_fault_table[fault].fn) && fault != FAULT_OBJECT) {
		debug_printf("Unhandled exception: %d", fault);
		twz_thread_exit();
	}
	if(_fault_table[fault].fn) {
		_fault_table[fault].fn(fault, _info);
	}
}

/* TODO: arch-dep */

/* Stack comes in as mis-aligned (like any function call),
 * so maintain that alignment until the call below. */
asm (" \
		.extern __twz_fault_entry_c ;\
		__twz_fault_entry: ;\
			pushq %rax;\
			pushq %rbx;\
			pushq %rcx;\
			pushq %rdx;\
			pushq %rbp;\
			pushq %r8;\
			pushq %r9;\
			pushq %r10;\
			pushq %r11;\
			pushq %r12;\
			pushq %r13;\
			pushq %r14;\
			pushq %r15;\
\
			call __twz_fault_entry_c ;\
\
			popq %r15;\
			popq %r14;\
			popq %r13;\
			popq %r12;\
			popq %r11;\
			popq %r10;\
			popq %r9;\
			popq %r8;\
			popq %rbp;\
			popq %rdx;\
			popq %rcx;\
			popq %rbx;\
			popq %rax;\
\
			popq %rdi;\
			popq %rsi;\
			popq %rsp;\
\
			subq $8, %rsp;\
			retq;\
	");
/* the subq is to move the stack back to where we stored the RIP for return.
 * We had to store the unmodified rsp from the kernel's frame to reload it
 * accurately, but the unmodified rsp is 8 above the location where we put the
 * return address */

void __twz_fault_entry(void);
void __twz_fault_init(void)
{
	/* have to do this manually, because fault handling during init
	 * may not use any global data (since the data object may not be mapped) */
	struct object thrd;
	twz_object_init(&thrd, TWZSLOT_THRD);
	struct twzthread_repr *repr = thrd.base;
	repr->thread_kso_data.faults[FAULT_OBJECT] = (struct faultinfo) {
		.addr = (void *)__twz_fault_entry,
	};
}

void twz_fault_set(int fault, void (*fn)(int, void *))
{
	bool needkso = _fault_table[fault].fn == 0;
	_fault_table[fault].fn = fn;

	if(needkso) {
		struct object thrd;
		twz_object_init(&thrd, TWZSLOT_THRD);
		struct twzthread_repr *repr = thrd.base;
		repr->thread_kso_data.faults[fault] = (struct faultinfo) {
			.addr = (void *)__twz_fault_entry,
		};
	}
}

