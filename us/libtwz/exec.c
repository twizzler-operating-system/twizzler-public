#include <string.h>
#include <twz/obj.h>
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

__attribute__((used)) static int __do_exec(uint64_t idlo,
  uint64_t idhi,
  uint64_t vidlo,
  uint64_t vidhi,
  void *vector)
{
	objid_t id = MKID(idhi, idlo);
	objid_t vid = MKID(vidhi, vidlo);
	struct object exe;
	int r;
	if((r = twz_object_open(&exe, id, FE_READ))) {
		return r;
	}

	/* TODO: set fixed-point for stack */
	struct elf64_header *hdr = twz_obj_base(&exe);

	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = hdr->e_entry,
		.rdi = (long)vector,
		.rsp = (long)SLOT_TO_VADDR(TWZSLOT_STACK) + 0x200000,
	};
	sys_become(0, &ba);
	twz_thread_exit();
	return 0;
}

int twz_exec(objid_t id, char const *const *argv, char *const *env)
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

	/* calculate space */
	size_t str_space = 0;
	size_t argc = 0;
	size_t envc = 0;
	for(const char *const *s = &argv[0]; *s; s++) {
		str_space += strlen(*s) + 1;
		argc++;
	}

	for(char *const *s = &env[0]; *s; s++) {
		str_space += strlen(*s) + 1;
		envc++;
	}

	sp = OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + 0x4000 + OBJ_NULLPAGE_SIZE);
	str_space = ((str_space - 1) & ~15) + 16;

	/* 4 for: 1 NULL each for argv and env, argc, and final null after env */
	long *vector_off = (void *)(sp - (str_space + (argc + envc + 4) * sizeof(char *)));
	long *vector = twz_ptr_lea(&stack, vector_off);

	size_t v = 0;
	vector[v++] = argc;
	char *str = sp + (char *)twz_obj_base(&stack);
	for(size_t i = 0; i < argc; i++) {
		const char *s = argv[i];
		str -= strlen(s) + 1;
		strcpy(str, s);
		vector[v++] = (long)twz_ptr_rebase(TWZSLOT_STACK, str);
	}
	vector[v++] = 0;

	for(size_t i = 0; i < envc; i++) {
		const char *s = env[i];
		str -= strlen(s) + 1;
		strcpy(str, s);
		vector[v++] = (long)twz_ptr_rebase(TWZSLOT_STACK, str);
	}
	vector[v++] = 0;
	vector[v++] = 0;

	/* TODO: we should really do this in assembly */
	struct twzthread_repr *repr = twz_thread_repr_base();
	repr->fixed_points[TWZSLOT_STACK] = (struct viewentry){
		.id = sid,
		.flags = VE_READ | VE_WRITE | VE_VALID,
	};

	/* TODO: copy-in everything for the vector */
	int ret;
	uint64_t p = (uint64_t)SLOT_TO_VADDR(TWZSLOT_STACK) + (OBJ_NULLPAGE_SIZE + 0x200000);
	register long r8 asm("r8") = (long)vector_off + (long)SLOT_TO_VADDR(TWZSLOT_STACK);
	__asm__ __volatile__("movq %%rax, %%rsp\n"
	                     "call __do_exec\n"
	                     : "=a"(ret)
	                     : "a"(p),
	                     "D"((uint64_t)id),
	                     "S"((uint64_t)(id >> 64)),
	                     "d"((uint64_t)vid),
	                     "c"((uint64_t)(vid >> 64)),
	                     "r"(r8));
	twz_thread_exit();
	return ret;
}
