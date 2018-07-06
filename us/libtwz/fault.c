#include <debug.h>

#include <twzfault.h>










#include <stdatomic.h>
#include <twz.h>
#include <twzobj.h>
#include <twzview.h>
#include <twzsys.h>
#include <twzerr.h>
#include <twzname.h>

struct twix_register_frame {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
};

long __twix_syscall_target_c(long num, struct twix_register_frame *frame)
{
	debug_printf("TWIX entry: %ld, %p %lx\n", num, frame, frame->rsp);
	return 0;
}

asm (
	".global __twix_syscall_target;"
	"__twix_syscall_target:;"
	"movq %rsp, %rcx;"
	"andq $-16, %rsp;"
	"pushq %rcx;"

	"pushq %rbx;"
	"pushq %rdx;"
	"pushq %rdi;"
	"pushq %rsi;"
	"pushq %rbp;"
	"pushq %r8;"
	"pushq %r9;"
	"pushq %r10;"
	"pushq %r11;"
	"pushq %r12;"
	"pushq %r13;"
	"pushq %r14;"
	"pushq %r15;"

	"movq %rsp, %rsi;"
	"movq %rax, %rdi;"
	"call __twix_syscall_target_c;"

	"popq %r15;"
	"popq %r14;"
	"popq %r13;"
	"popq %r12;"
	"popq %r11;"
	"popq %r10;"
	"popq %r9;"
	"popq %r8;"
	"popq %rbp;"
	"popq %rsi;"
	"popq %rdi;"
	"popq %rdx;"
	"popq %rbx;"
	"popq %rsp;"
	"ret;"
);

int twz_map_fot_entry(size_t slot, struct fotentry *fe)
{
	objid_t id;
	if(fe->flags & FE_NAME) {
		id = twz_name_resolve(NULL, fe->data, NAME_RESOLVER_DEFAULT);
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
	uint64_t offset = addr % (1024ul * 1024 * 1024);
	if(offset < 0x1000) {
		debug_printf("NULL ptr\n");
		return TE_FAILURE;
	}

	if(!(cause & FAULT_OBJECT_NOMAP)) {
		debug_printf("PERM err\n");
		return TE_FAILURE;
	}

	/* TODO: check if there is an object 0 */

	struct metainfo *mi = (void *)(1024ul * 1024 * 1024 - 0x1000);
	if(mi->magic != MI_MAGIC) {
		debug_printf("No metainfo in object 0\n");
		return TE_FAILURE;
	}

	if(!(mi->flags & MIF_FOT)) {
		debug_printf("Object 0 has no FOT\n");
		return TE_FAILURE;
	}

	size_t slot = (addr / (1024ul * 1024 * 1024)) - 1;
	if(slot >= 0x10000) {
		struct fotentry _fot_derive_rw = {
			.id = 0, .flags = FE_READ | FE_WRITE | FE_DERIVE
		};
		switch(slot+1) {
			case TWZSLOT_VCACHE:
				return twz_map_fot_entry(slot+1, &_fot_derive_rw);
		}
		debug_printf("Fault in allocatable object space (%lx)\n", slot+1);
		return TE_FAILURE;
	}
	if(slot >= 94) {
		debug_printf("TODO: large FOTs (%lx)\n", slot);
		return TE_FAILURE;
	}

	struct fotentry *fot = (struct fotentry *)((char *)mi + 1024);

	if(fot[slot].id == 0) {
		debug_printf("Invalid FOT entry\n");
		return TE_FAILURE;
	}

	debug_printf("Slot: %ld - %s\n", slot+1, fot[slot].data);

	return twz_map_fot_entry(slot+1, &fot[slot]);
}














#include <twzthread.h>
static __attribute__((used)) void __twz_fault_entry_c(int fault, void *_info)
{
	struct fault_object_info *info = _info;
	debug_printf("FAULT :: %d %lx %lx %lx", fault, info->ip, info->addr, info->flags);
	if(twz_handle_fault(info->addr, info->flags, info->ip) == TE_FAILURE) {
		twz_thread_exit();
	}
	debug_printf("Handled");
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

#include <twzobj.h>
#include <twzslots.h>
#include <twzthread.h>

void __twz_fault_entry(void);

void __twz_fault_init(void)
{
	struct object thrd;
	twz_object_init(&thrd, TWZSLOT_THRD);
	struct twzthread_repr *repr = thrd.base;
	repr->thread_kso_data.faults[FAULT_OBJECT] = (struct faultinfo) {
		.addr = (void *)__twz_fault_entry,
	};
}

