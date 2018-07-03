#pragma once

#include <elf.h>
#include <twzobj.h>
#include <twzviewcall.h>
#include <twzthread.h>

#include <debug.h>

struct twzexecinfo {
	objid_t target;
	objid_t sc;
};

static inline int twz_exec(objid_t id)
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
	long x[] = {0, (long)&y, (long)&y, 0};
	return twz_viewcall((void *)entry, vid, 0, (long)x, (long)x, x, x, x, x);
}

static inline int __do_exec(objid_t sc, objid_t vid, long *jmp)
{
	long a1 = ID_LO(sc);
	long a2 = ID_HI(sc);
	long a3 = ID_LO(vid);
	long a4 = ID_HI(vid);
	unsigned long ret;
	char err = 0;
	register long r10 __asm__("r10") = a4;
	register long r8 __asm__("r8") = jmp;
	long n = SYS_twistie_become;
	register long r9 __asm__("r9") = jmp[1];
	__asm__ __volatile__ ("push %%r9; xor %%r9, %%r9; syscall; setb %%cl" : "=a"(ret), "=c"(err) : "a"(n), "D"(a1), "S"(a2),
			"d"(a3), "r"(r10), "r"(r8), "r"(r9) : "r11", "memory");
	if(err) {
		return -ret;
	}
	return ret;           

}

static inline int twz_exec2(objid_t id, objid_t sc)
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
	return __do_exec(sc, vid, (long *)jmp);
}

