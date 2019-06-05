#include <twz/_obj.h>
#include <twz/_objid.h>
#include <twz/debug.h>
#include <twz/thread.h>
#include <twz/view.h>

static struct {
	void (*fn)(int, void *);
} _fault_table[NUM_FAULTS];

#define TE_FAILURE -1
#define TE_SUCCESS 0

int twz_map_fot_entry(size_t slot, struct fotentry *fe)
{
	objid_t id;
	if(fe->flags & FE_NAME) {
		//	id = twz_name_resolve(NULL, fe->data, fe->nresolver);
		//	if(id == 0) {
		debug_printf("Name not resolved: %s\n", fe->name.data);
		//		return TE_FAILURE;
		//	}
	} else {
		id = fe->id;
	}

	int flags = ((fe->flags & FE_READ) ? VE_READ : 0) | ((fe->flags & FE_WRITE) ? VE_WRITE : 0)
	            | ((fe->flags & FE_EXEC) ? VE_EXEC : 0);

#if 0
	if(fe->flags & FE_DERIVE) {
		objid_t nid = 0;
		if(twz_object_new(
		     NULL, &nid, id, 0 /* TODO: KUID, perms */, TWZ_ON_DFL_READ | TWZ_ON_DFL_WRITE)
		   < 0) {
			debug_printf("Failed to make new derived object\n");
			return TE_FAILURE;
		}
		id = nid;
	}
#endif

	twz_view_set(NULL, slot, id, flags);
	debug_printf("Mapping slot: %ld :: " IDFMT "\n", slot, IDPR(id));

	return TE_SUCCESS;
}

int twz_handle_fault(uintptr_t addr, int cause, uintptr_t source)
{
	uint64_t offset = addr % OBJ_MAXSIZE;
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

	// struct metainfo *mi = twz_slot_to_meta(0);
	struct metainfo *mi = (void *)(OBJ_MAXSIZE - OBJ_METAPAGE_SIZE);
	if(mi->magic != MI_MAGIC) {
		debug_printf("No metainfo in object 0\n");
		return TE_FAILURE;
	}

	size_t slot = (addr / OBJ_MAXSIZE);
	// if(slot >= TWZSLOT_ALLOC_START) {
	//	debug_printf("Fault in allocatable object space (%lx)\n", slot + 1);
	//	return TE_FAILURE;
	//}
	if(slot >= 94) {
		debug_printf("TODO: large FOTs (%lx)\n", slot);
		return TE_FAILURE;
	}

	// struct object obj0 = TWZ_OBJECT_INIT(0);
	// struct fotentry *fot = twz_object_fot(&obj0, false);
	// if(!fot) {
	//	debug_printf("obj 0 has no FOT\n");
	//}

	struct fotentry *fot = (void *)((char *)mi + mi->milen);

	if(fot[slot].id == 0) {
		debug_printf("Invalid FOT entry\n");
		return TE_FAILURE;
	}

	if(fot[slot].flags & FE_NAME) {
		debug_printf("Slot: %ld - %s\n", slot, fot[slot].name.data);
	} else {
		debug_printf("Slot: %ld - " IDFMT "\n", slot, IDPR(fot[slot].id));
	}

	return twz_map_fot_entry(slot, &fot[slot]);
}

int __fault_obj_default(int fault, struct fault_object_info *info)
{
	return twz_handle_fault(info->addr, info->flags, info->ip);
}

static __attribute__((used)) void __twz_fault_entry_c(int fault, void *_info)
{
	if(fault == FAULT_OBJECT) {
		struct fault_object_info *fi = _info;
		debug_printf("FIO: %lx (%p)\n", fi->addr, &_fault_table[fault]);
		if(fi->addr == (uintptr_t)&_fault_table[fault] || !_fault_table[fault].fn) {
			if(__fault_obj_default(fault, _info) < 0) {
				twz_thread_exit();
			}
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
asm(" \
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
	uint64_t s, d;
	asm volatile("mov %%fs:0, %%rsi" : "=S"(s));
	asm volatile("mov %%fs:8, %%rdx" : "=d"(d));

	__seg_gs struct twzthread_repr *repr = (uintptr_t)OBJ_NULLPAGE_SIZE;

	/* have to do this manually, because fault handling during init
	 * may not use any global data (since the data object may not be mapped) */

	repr->faults[FAULT_OBJECT] = (struct faultinfo){
		.addr = (void *)__twz_fault_entry,
	};
}
