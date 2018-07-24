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

	char **argv = {NULL};
	long x[] = {0, (long)argv, (long)argv, 0};

	char *argbase = STACK_BASE + STACK_SIZE;



	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = entry,
		.rdi = (long)x,
		.rsp = (uintptr_t)STACK_BASE + STACK_SIZE,
	};
	return sys_become(sc, &ba);
}

static inline int twz_execv(objid_t id, objid_t sc, char *const argv[])
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

	char *argbase = STACK_BASE + STACK_SIZE + TLS_SIZE;
	int argc;
	for(argc=0;argv[argc];argc++);

	long cpy[argc + 2];
	cpy[0] = argc;
	for(int i=0;i<argc;i++) {
		size_t len = strlen(argv[i]);
		strcpy(argbase, argv[i]);
		cpy[i+1] = (long)argbase;
		argbase += (len + 1);
	}
	cpy[argc+1] = 0;
	argbase += sizeof(long);
	argbase = (char *)((uintptr_t)argbase & ~(sizeof(long) - 1));
	memcpy(argbase, cpy, sizeof(cpy[0]) * (argc + 2));

	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = entry,
		.rdi = (long)argbase,
		.rsp = (uintptr_t)STACK_BASE + STACK_SIZE,
	};
	return sys_become(sc, &ba);
}

