#pragma once

#include <elf.h>
#include <twzobj.h>
#include <twzviewcall.h>
#include <twzthread.h>

static inline int twz_exec(objid_t id, objid_t sc)
{
	struct object vb;
	twz_view_blank(&vb);
	struct object thrd;
	twz_view_set(&vb, 0, id, VE_READ | VE_EXEC);
	objid_t vid = twz_object_getid(&vb);

	struct object exe;
	twz_object_open(&exe, id, FE_READ);
	Elf64_Ehdr *hdr = __twz_ptr_lea(&exe, (void *)OBJ_NULLPAGE_SIZE);

	uintptr_t entry = hdr->e_entry;

	twz_view_copy(&vb, NULL, TWZSLOT_STDIN);
	twz_view_copy(&vb, NULL, TWZSLOT_STDOUT);
	twz_view_copy(&vb, NULL, TWZSLOT_STDERR);

	long y=0;
	char **argv = {NULL};
	long x[] = {0, (long)argv, (long)argv, 0};
	uint64_t jmp[] = {
		entry, (uint64_t)x, (uint64_t)x, (uint64_t)x, (uint64_t)x, (uint64_t)x,
		(uint64_t)x, 0
	};

	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = entry,
		.rdi = (long)x,
		.rsp = STACK_BASE + STACK_SIZE,
	};
	return sys_become(sc, &ba);
}

