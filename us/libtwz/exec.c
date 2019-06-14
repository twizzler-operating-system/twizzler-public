#include <stdlib.h>
#include <twz/sys.h>
#include <twz/thread.h>
#include <twz/view.h>
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

extern char **environ;

static int __do_exec(uint64_t idlo, uint64_t idhi, uint64_t vidlo, uint64_t vidhi, void *vector)
{
	struct object exe;
	if((r = twz_object_open(&exe, id, FE_READ | FE_WRITE))) {
		return r;
	}

	/* TODO: set fixed-point for stack */
	struct elf64_header *hdr = twz_obj_base(&exe);

	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = hdr->e_entry,
		.rdi = 0,
	};
	return sys_become(0, &ba);
}

int twz_exec(objid_t id, char **argv, char **env)
{
	if(env == NULL)
		env = environ;

	objid_t vid;
	int r;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &vid))) {
		return r;
	}
	struct object view;
	if((r = twz_object_open(&view, vid, FE_READ | FE_WRITE))) {
		return r;
	}

	struct object stack;
	objid_t sid;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &sid))) {
		return r;
	}
	if((r = twz_object_open(&stack, sid, FE_READ | FE_WRITE))) {
		return r;
	}

	if((r = twz_view_set(&view, TWZSLOT_CVIEW, vid, VE_READ | VE_WRITE))) {
		return r;
	}

	if((r = twz_view_set(&view, 0, id, VE_READ | VE_EXEC))) {
		return r;
	}

	uint64_t sp;
	uint64_t vector;
	/* TODO: copy-in everything for the vector */
	int ret;
	asm volatile("movq %%rax, %%rsp\n"
	             "call __do_exec\n"
	             "=a"(ret)
	             : "a"(sp),
	             "D"((uint64_t)id),
	             "S"((uint64_t)(id >> 64)),
	             "d"((uint64_t)vid),
	             "c"((uint64_t)(vid >> 64)),
	             "r8"(vector));
	return ret;
}
