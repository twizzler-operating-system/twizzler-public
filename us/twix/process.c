#include <errno.h>
#include <string.h>
#include <twz/name.h>
#include <twz/sys.h>
#include <twz/thread.h>
#include <twz/view.h>

#include "syscalls.h"

struct process {
	struct thread thrd;
	int pid;
};

#define MAX_PID 1024
static struct process pds[MAX_PID];

#include <elf.h>

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

extern int *__errno_location();
#include <twz/debug.h>
__attribute__((used)) static int __do_exec(uint64_t entry,
  uint64_t _flags,
  uint64_t vidlo,
  uint64_t vidhi,
  void *vector)
{
	(void)_flags;
	objid_t vid = MKID(vidhi, vidlo);

	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = entry,
		.rdi = (long)vector,
		.rsp = (long)SLOT_TO_VADDR(TWZSLOT_STACK) + 0x200000,
	};
	int r = sys_become(&ba);
	twz_thread_exit(r);
	return 0;
}

extern char **environ;
static int __internal_do_exec(twzobj *view,
  size_t entry,
  char const *const *argv,
  char *const *env,
  void *auxbase,
  void *phdr,
  size_t phnum,
  size_t phentsz,
  void *auxentry)
{
	if(env == NULL)
		env = environ;

	twzobj stack;
	objid_t sid;
	int r;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &sid))) {
		return r;
	}
	if((r = twz_object_init_guid(&stack, sid, FE_READ | FE_WRITE))) {
		return r;
	}
	if((r = twz_object_tie(view, &stack, 0))) {
		return r;
	}
	if((r = twz_object_delete_guid(sid, 0))) {
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

	sp = OBJ_TOPDATA;
	str_space = ((str_space - 1) & ~15) + 16;

	/* TODO: check if we have enough space... */

	/* 4 for: 1 NULL each for argv and env, argc, and final null after env */
	long *vector_off = (void *)(sp
	                            - (str_space + (argc + envc + 4) * sizeof(char *)
	                               + sizeof(long) * 2 * 32 /* TODO: number of aux vectors */));
	long *vector = twz_object_lea(&stack, vector_off);

	size_t v = 0;
	vector[v++] = argc;
	char *str = (char *)twz_object_lea(&stack, (char *)sp);
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

	vector[v++] = AT_BASE;
	vector[v++] = (long)auxbase;

	vector[v++] = AT_PAGESZ;
	vector[v++] = 0x1000;

	vector[v++] = AT_ENTRY;
	vector[v++] = (long)auxentry;

	vector[v++] = AT_PHNUM;
	vector[v++] = (long)phnum;

	vector[v++] = AT_PHENT;
	vector[v++] = (long)phentsz;

	vector[v++] = AT_PHDR;
	vector[v++] = (long)phdr;

	vector[v++] = AT_NULL;
	vector[v++] = AT_NULL;

	/* TODO: we should really do this in assembly */
	struct twzthread_repr *repr = twz_thread_repr_base();
	repr->fixed_points[TWZSLOT_STACK] = (struct viewentry){
		.id = sid,
		.flags = VE_READ | VE_WRITE | VE_VALID,
	};

	memset(repr->faults, 0, sizeof(repr->faults));
	objid_t vid = twz_object_guid(view);

	/* TODO: copy-in everything for the vector */
	int ret;
	uint64_t p = (uint64_t)SLOT_TO_VADDR(TWZSLOT_STACK) + (OBJ_NULLPAGE_SIZE + 0x200000);
	register long r8 asm("r8") = (long)vector_off + (long)SLOT_TO_VADDR(TWZSLOT_STACK);
	__asm__ __volatile__("movq %%rax, %%rsp\n"
	                     "call __do_exec\n"
	                     : "=a"(ret)
	                     : "a"(p),
	                     "D"((uint64_t)entry),
	                     "S"((uint64_t)(0)),
	                     "d"((uint64_t)vid),
	                     "c"((uint64_t)(vid >> 64)),
	                     "r"(r8));
	twz_thread_exit(ret);
	return ret;
}

static int __internal_load_elf_interp(twzobj *view, twzobj *elfobj, void **base, void **entry)
{
	Elf64_Ehdr *hdr = twz_object_base(elfobj);

	twzobj new_text, new_data;
	int r;
	if((r = twz_object_new(&new_text,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC | TWZ_OC_VOLATILE))) {
		return r;
	}
	if((r = twz_object_new(
	      &new_data, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE))) {
		return r;
	}

	char *phdr_start = (char *)hdr + hdr->e_phoff;
	for(unsigned i = 0; i < hdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (void *)(phdr_start + i * hdr->e_phentsize);
		if(phdr->p_type == PT_LOAD) {
			twix_log("load: off=%lx, vaddr=%lx, paddr=%lx, fsz=%lx, msz=%lx\n",
			  phdr->p_offset,
			  phdr->p_vaddr,
			  phdr->p_paddr,
			  phdr->p_filesz,
			  phdr->p_memsz);
			//	twix_log("  -> %lx %lx\n",
			//	  phdr->p_vaddr & ~(phdr->p_align - 1),
			//	  phdr->p_offset & ~(phdr->p_align - 1));
#if 0
			char *memstart = twz_object_base(&newobj);
			char *filestart = twz_object_base(elfobj);
			memstart += phdr->p_vaddr & ~(phdr->p_align - 1);
			filestart += phdr->p_offset & ~(phdr->p_align - 1);
			size_t len = phdr->p_filesz;
			len += (phdr->p_offset & (phdr->p_align - 1));
			twix_log(":: %p %p %lx\n", memstart, filestart, len);
			memcpy(memstart, filestart, len);
#endif
			twzobj *to;
			if(phdr->p_flags & PF_X) {
				to = &new_text;
			} else {
				to = &new_data;
			}

			char *memstart = twz_object_base(to);
			char *filestart = twz_object_base(elfobj);
			memstart +=
			  ((phdr->p_vaddr & ~(phdr->p_align - 1)) % OBJ_MAXSIZE); // - OBJ_NULLPAGE_SIZE;
			filestart += phdr->p_offset & ~(phdr->p_align - 1);
			size_t len = phdr->p_filesz;
			len += (phdr->p_offset & (phdr->p_align - 1));
			twix_log("  ==> %p %p %lx\n", filestart, memstart, len);
			if((r = sys_ocopy(twz_object_guid(to),
			      twz_object_guid(elfobj),
			      (long)memstart % OBJ_MAXSIZE,
			      (long)filestart % OBJ_MAXSIZE,
			      (len + 0xfff) & ~0xfff,
			      0))) {
				twix_log("oc: %d\n", r);
				return r;
			}

			//		memcpy(memstart, filestart, len);
		}
	}

	twz_object_tie(view, &new_text, 0);
	twz_object_tie(view, &new_data, 0);

	size_t base_slot = 0x10003;
	twz_view_set(view, base_slot, twz_object_guid(&new_text), VE_READ | VE_EXEC);
	twz_view_set(view, base_slot + 1, twz_object_guid(&new_data), VE_READ | VE_WRITE);

	/* TODO: actually do tying */
	// twz_object_tie(view, &newobj, 0);

	// twz_view_set(view, base_slot, twz_object_guid(&newobj), VE_READ | VE_WRITE | VE_EXEC);

	*base = (void *)(SLOT_TO_VADDR(base_slot) + OBJ_NULLPAGE_SIZE);
	*entry = (char *)*base + hdr->e_entry;

	return 0;
}

static int __internal_load_elf_exec(twzobj *view, twzobj *elfobj, void **phdr, void **entry)
{
	Elf64_Ehdr *hdr = twz_object_base(elfobj);

	twzobj new_text, new_data;
	int r;
	if((r = twz_object_new(&new_text,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC | TWZ_OC_VOLATILE))) {
		return r;
	}
	if((r = twz_object_new(
	      &new_data, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE))) {
		return r;
	}

	char *phdr_start = (char *)hdr + hdr->e_phoff;
	for(unsigned i = 0; i < hdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (void *)(phdr_start + i * hdr->e_phentsize);
		if(phdr->p_type == PT_LOAD) {
			twix_log("load: off=%lx, vaddr=%lx, paddr=%lx, fsz=%lx, msz=%lx\n",
			  phdr->p_offset,
			  phdr->p_vaddr,
			  phdr->p_paddr,
			  phdr->p_filesz,
			  phdr->p_memsz);
			twix_log("  -> %lx %lx\n",
			  phdr->p_vaddr & ~(phdr->p_align - 1),
			  phdr->p_offset & ~(phdr->p_align - 1));

			twzobj *to;
			if(phdr->p_flags & PF_X) {
				to = &new_text;
			} else {
				to = &new_data;
			}

			char *memstart = twz_object_base(to);
			char *filestart = twz_object_base(elfobj);
			memstart += ((phdr->p_vaddr & ~(phdr->p_align - 1)) % OBJ_MAXSIZE) - OBJ_NULLPAGE_SIZE;
			filestart += phdr->p_offset & ~(phdr->p_align - 1);
			size_t len = phdr->p_filesz;
			len += (phdr->p_offset & (phdr->p_align - 1));
			//	twix_log("  ==> %p %p %lx\n", filestart, memstart, len);
			if((r = sys_ocopy(twz_object_guid(to),
			      twz_object_guid(elfobj),
			      (long)memstart % OBJ_MAXSIZE,
			      (long)filestart % OBJ_MAXSIZE,
			      (len + 0xfff) & ~0xfff,
			      0))) {
				twix_log("oc: %d\n", r);
				return r;
			}
			//			memcpy(memstart, filestart, len);
		}
	}

	/* TODO: actually do tying */
	twz_object_tie(view, &new_text, 0);
	twz_object_tie(view, &new_data, 0);

	twz_view_set(view, 0, twz_object_guid(&new_text), VE_READ | VE_EXEC);
	twz_view_set(view, 1, twz_object_guid(&new_data), VE_READ | VE_WRITE);

	*phdr = (void *)(OBJ_NULLPAGE_SIZE + hdr->e_phoff);
	*entry = (char *)hdr->e_entry;

	return 0;
}

static long __internal_execve_view_interp(twzobj *view,
  twzobj *exe,
  const char *interp_path,
  const char *const *argv,
  char *const *env)
{
	twzobj interp;
	Elf64_Ehdr *hdr = twz_object_base(exe);
	int r;
	if((r = twz_object_init_name(&interp, interp_path, FE_READ))) {
		return r;
	}

	void *interp_base, *interp_entry;
	if((r = __internal_load_elf_interp(view, &interp, &interp_base, &interp_entry))) {
		return r;
	}

	void *exe_entry, *phdr;
	if((r = __internal_load_elf_exec(view, exe, &phdr, &exe_entry))) {
		return r;
	}

	// twix_log("GOT interp base=%p, entry=%p\n", interp_base, interp_entry);
	// twix_log("GOT phdr=%p, entry=%p\n", phdr, exe_entry);

	__internal_do_exec(
	  view, interp_entry, argv, env, interp_base, phdr, hdr->e_phnum, hdr->e_phentsize, exe_entry);
	return -1;
}

long linux_sys_execve(const char *path, const char *const *argv, char *const *env)
{
	objid_t id = 0;
	int r = twz_name_resolve(NULL, path, NULL, 0, &id);
	if(r) {
		return r;
	}

	objid_t vid;
	twzobj view;
	if((r = twz_exec_create_view(&view, id, &vid)) < 0) {
		return r;
	}

	twix_copy_fds(&view);

	twzobj exe;
	twz_object_init_guid(&exe, id, FE_READ);
	struct elf64_header *hdr = twz_object_base(&exe);

	kso_set_name(NULL, "[instance] [unix] %s", path);

	char *phdr_start = (char *)hdr + hdr->e_phoff;
	for(unsigned i = 0; i < hdr->e_phnum; i++) {
		Elf64_Phdr *phdr = (void *)(phdr_start + i * hdr->e_phentsize);
		if(phdr->p_type == PT_INTERP) {
			char *interp = (char *)hdr + phdr->p_offset;
			//	twix_log("INTERPRETER: %s\n", interp);
			return __internal_execve_view_interp(
			  &view, &exe, /* TODO */ "/usr/lib/libc.so", argv, env);
			return -1;
		}
	}

	r = twz_exec_view(&view, vid, hdr->e_entry, argv, env);

	return r;
}

asm(".global __return_from_clone\n"
    "__return_from_clone:"
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
    "popq %rax;" /* ignore the old rsp */
    "movq $0, %rax;"
    "ret;");

asm(".global __return_from_fork\n"
    "__return_from_fork:"
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
    "movq $0, %rax;"
    "ret;");

extern uint64_t __return_from_clone(void);
extern uint64_t __return_from_fork(void);
long linux_sys_clone(struct twix_register_frame *frame,
  unsigned long flags,
  void *child_stack,
  int *ptid,
  int *ctid,
  unsigned long newtls)
{
	(void)ptid;
	(void)ctid;
	if(flags != 0x7d0f00) {
		return -ENOSYS;
	}

	memcpy((void *)((uintptr_t)child_stack - sizeof(struct twix_register_frame)),
	  frame,
	  sizeof(struct twix_register_frame));
	child_stack = (void *)((uintptr_t)child_stack - sizeof(struct twix_register_frame));
	/* TODO: track these, and when these exit or whatever, release these as well */
	struct thread thr;
	int r;
	if((r = twz_thread_spawn(&thr,
	      &(struct thrd_spawn_args){ .start_func = (void *)__return_from_clone,
	        .arg = NULL,
	        .stack_base = child_stack,
	        .stack_size = 8,
	        .tls_base = (char *)newtls }))) {
		return r;
	}

	/* TODO */
	static _Atomic int __static_thrid = 0;
	return ++__static_thrid;
}

#include <sys/mman.h>

struct mmap_slot {
	twzobj obj;
	int prot;
	int flags;
	size_t slot;
	struct mmap_slot *next;
};

#include <twz/mutex.h>
static struct mutex mmap_mutex;
static uint8_t mmap_bitmap[TWZSLOT_MMAP_NUM / 8];

static ssize_t __twix_mmap_get_slot(void)
{
	for(size_t i = 0; i < TWZSLOT_MMAP_NUM; i++) {
		if(!(mmap_bitmap[i / 8] & (1 << (i % 8)))) {
			mmap_bitmap[i / 8] |= (1 << (i % 8));
			return i;
		}
	}
	return -1;
}

static int __twix_mmap_take_slot(size_t slot)
{
	if(mmap_bitmap[slot / 8] & (1 << (slot % 8))) {
		return -1;
	}
	mmap_bitmap[slot / 8] |= (1 << (slot % 8));
	return 0;
}

static long __internal_mmap_object(void *addr, size_t len, int prot, int flags, int fd, size_t off)
{
	struct file *file = twix_get_fd(fd);
	if(!file)
		return -EBADF;
	int r;
	twzobj newobj;
	twzobj *obj;
	/* TODO: verify perms */
	uint64_t fe = 0;
	if(prot & PROT_READ)
		fe |= FE_READ;
	if(prot & PROT_WRITE)
		fe |= FE_WRITE;
	if(prot & PROT_EXEC)
		fe |= FE_EXEC;

	if(len > OBJ_TOPDATA) {
		len = OBJ_TOPDATA;
	}
	size_t adj = 0;
	if(flags & MAP_PRIVATE) {
		ssize_t slot = addr ? __twix_mmap_take_slot(VADDR_TO_SLOT(addr)) : __twix_mmap_get_slot();
		if(slot < 0) {
			return -ENOMEM;
		}

		if(addr) {
			if((long)addr % OBJ_MAXSIZE < OBJ_NULLPAGE_SIZE) {
				return -EINVAL;
			}
			adj = ((long)addr % OBJ_MAXSIZE) - OBJ_NULLPAGE_SIZE;
		}

		if((r = twz_object_new(&newobj,
		      NULL,
		      NULL,
		      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC /* TODO */ | TWZ_OC_VOLATILE
		        | TWZ_OC_TIED_VIEW))) {
			return r;
		}

		if((r = sys_ocopy(twz_object_guid(&newobj),
		      twz_object_guid(&file->obj),
		      OBJ_NULLPAGE_SIZE,
		      off + OBJ_NULLPAGE_SIZE,
		      (len + 0xfff) & ~0xfff,
		      0))) {
			twix_log("ocopy failed: %d\n", r);
			return r;
		}

		obj = &newobj;
		/*	if(off) {
		        char *src = twz_object_base(&file->obj);
		        char *dst = twz_object_base(obj);

		        memcpy(dst, src + off, len);
		    }*/

	} else {
		obj = &file->obj;
		adj = off;
	}
	return (long)twz_object_base(obj) + adj;
}

long linux_sys_mmap(void *addr, size_t len, int prot, int flags, int fd, size_t off)
{
	(void)prot;
	(void)off;
	// twix_log("sys_mmap: %p %lx %x %x %d %lx\n", addr, len, prot, flags, fd, off);
	if(fd >= 0) {
		long ret = __internal_mmap_object(addr, len, prot, flags, fd, off);
		//	twix_log("      ==>> %lx\n", ret);
		return ret;
	}
	if(addr != NULL && (flags & MAP_FIXED)) {
		return -ENOTSUP;
	}
	if(!(flags & MAP_ANON)) {
		return -ENOTSUP;
	}

	/* TODO: fix all this up so its better */
	size_t slot = 0x10006ul;
	objid_t o;
	uint32_t fl;
	twz_view_get(NULL, slot, &o, &fl);
	if(!(fl & VE_VALID)) {
		objid_t nid = 0;
		if(twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_VIEW, 0, 0, &nid)) {
			return -ENOMEM;
		}
		twz_view_set(NULL, slot, nid, FE_READ | FE_WRITE);
	}

	void *base = (void *)(slot * 1024 * 1024 * 1024 + 0x1000);
	struct metainfo *mi = (void *)((slot + 1) * 1024 * 1024 * 1024 - 0x1000);
	uint32_t *next = (uint32_t *)((char *)mi + mi->milen);
	if(*next + len > (1024 * 1024 * 1024 - 0x2000)) {
		return -1; // TODO allocate a new object
	}

	long ret = (long)(base + *next);
	*next += len;
	return ret;
}

#include <twz/thread.h>
long linux_sys_exit(int code)
{
	twz_thread_exit(code);
	return 0;
}

long linux_sys_set_tid_address()
{
	/* TODO: NI */
	return 0;
}

#include <twz/debug.h>

static bool __fork_view_clone(twzobj *nobj,
  size_t i,
  objid_t oid,
  uint32_t oflags,
  objid_t *nid,
  uint32_t *nflags)
{
	if(i == 0 || (i >= TWZSLOT_ALLOC_START && i <= TWZSLOT_ALLOC_MAX)) {
		*nid = oid;
		*nflags = oflags;
		return true;
	}

	return false;
}

long linux_sys_fork(struct twix_register_frame *frame)
{
	int r;
	twzobj view, cur_view;
	twz_view_object_init(&cur_view);

	// debug_printf("== creating view\n");
	if((r = twz_object_new(&view, &cur_view, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))) {
		return r;
	}

	objid_t vid = twz_object_guid(&view);

	/*if((r = twz_object_wire(NULL, &view)))
	    return r;
	if((r = twz_object_delete(&view, 0)))
	    return r;*/

	int pid = 0;
	for(int i = 1; i < MAX_PID; i++) {
		if(pds[i].pid == 0) {
			pid = i;
			break;
		}
	}

	if(pid == 0) {
		return -1;
	}
	pds[pid].pid = pid;
	// debug_printf("== creating thread\n");
	twz_thread_create(&pds[pid].thrd);

	twz_view_clone(NULL, &view, 0, __fork_view_clone);

	objid_t sid;
	twzobj stack;
	twz_view_fixedset(
	  &pds[pid].thrd.obj, TWZSLOT_THRD, pds[pid].thrd.tid, VE_READ | VE_WRITE | VE_FIXED);
	/* TODO: handle these */
	twz_object_wire_guid(&view, pds[pid].thrd.tid);

	twz_view_set(&view, TWZSLOT_CVIEW, vid, VE_READ | VE_WRITE);

	//	debug_printf("== creating stack\n");
	if((r = twz_object_new(&stack, twz_stdstack, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))) {
		twix_log(":: fork create stack returned %d\n", r);
		abort();
	}
	twz_object_tie(&pds[pid].thrd.obj, &stack, 0);
	sid = twz_object_guid(&stack);
	twz_view_fixedset(&pds[pid].thrd.obj, TWZSLOT_STACK, sid, VE_READ | VE_WRITE | VE_FIXED);
	// twz_object_wire_guid(&view, sid);

	size_t slots_to_copy[] = {
		1, TWZSLOT_UNIX, 0x10006 /* mmap */
	};
	for(size_t j = 0; j < sizeof(slots_to_copy) / sizeof(slots_to_copy[0]); j++) {
		size_t i = slots_to_copy[j];
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID)) {
			continue;
		}
		/* Copy-derive */
		objid_t nid;
		if((r = twz_object_create(
		      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC /* TODO */ | TWZ_OC_TIED_NONE,
		      0,
		      id,
		      &nid))) {
			/* TODO: cleanup */
			return r;
		}
		if(flags & VE_FIXED)
			twz_view_fixedset(&pds[pid].thrd.obj, i, nid, flags);
		else
			twz_view_set(&view, i, nid, flags);
		twz_object_wire_guid(&view, nid);
		twz_object_delete_guid(nid, 0);
	}

	// twz_object_wire(NULL, &stack);
	// twz_object_delete(&stack, 0);

	size_t soff = (uint64_t)twz_ptr_local(frame) - 1024;
	void *childstack = twz_object_lea(&stack, (void *)soff);

	memcpy(childstack, frame, sizeof(struct twix_register_frame));

	uint64_t fs;
	asm volatile("rdfsbase %%rax" : "=a"(fs));

	struct sys_thrd_spawn_args sa = {
		.target_view = vid,
		.start_func = (void *)__return_from_fork,
		.arg = NULL,
		.stack_base = (void *)twz_ptr_rebase(TWZSLOT_STACK, soff),
		.stack_size = 8,
		.tls_base = (void *)fs,
		.thrd_ctrl = TWZSLOT_THRD,
	};

	//	debug_printf("== spawning\n");
	if((r = sys_thrd_spawn(pds[pid].thrd.tid, &sa, 0))) {
		return r;
	}

	twz_object_tie(twz_stdthread, &view, TIE_UNTIE);
	twz_object_tie(twz_stdthread, &stack, TIE_UNTIE);
	// twz_object_unwire(NULL, &view);
	// twz_object_unwire(NULL, &stack);
	twz_object_release(&view);
	twz_object_release(&stack);

	return pid;
}

#include <twz/debug.h>
struct rusage;
long linux_sys_wait4(long pid, int *wstatus, int options, struct rusage *rusage)
{
	(void)pid;
	(void)options;
	(void)rusage;
	while(1) {
		struct thread *thrd[MAX_PID];
		int sps[MAX_PID];
		long event[MAX_PID] = { 0 };
		uint64_t info[MAX_PID];
		int pids[MAX_PID];
		size_t c = 0;
		for(int i = 0; i < MAX_PID; i++) {
			if(pds[i].pid) {
				sps[c] = THRD_SYNC_EXIT;
				pids[c] = i;
				thrd[c++] = &pds[i].thrd;
			}
		}
		int r = twz_thread_wait(c, thrd, sps, event, info);
		if(r < 0) {
			return r;
		}

		for(unsigned int i = 0; i < c; i++) {
			if(event[i] && pds[pids[i]].pid) {
				if(wstatus) {
					*wstatus = 0; // TODO
				}
				pds[pids[i]].pid = 0;
				twz_thread_release(&pds[pids[i]].thrd);
				return pids[i];
			}
		}
	}
}

#include <twz/debug.h>

#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_FD 2
#define FUTEX_REQUEUE 3
#define FUTEX_CMP_REQUEUE 4
#define FUTEX_WAKE_OP 5
#define FUTEX_LOCK_PI 6
#define FUTEX_UNLOCK_PI 7
#define FUTEX_TRYLOCK_PI 8
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAKE_BITSET 10
#define FUTEX_WAIT_REQUEUE_PI 11
#define FUTEX_CMP_REQUEUE_PI 12

#define FUTEX_PRIVATE_FLAG 128
#define FUTEX_CLOCK_REALTIME 256
#define FUTEX_CMD_MASK ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)

long linux_sys_futex(int *uaddr,
  int op,
  int val,
  const struct timespec *timeout,
  int *uaddr2,
  int val3)
{
	(void)timeout;
	(void)uaddr2;
	(void)val3;
	switch((op & FUTEX_CMD_MASK)) {
		case FUTEX_WAIT:
			return 0; // TODO
			break;
		case FUTEX_WAKE:
			return 0; // TODO
			break;
		default:
			twix_log("futex %d: %p (%x) %x\n", op, uaddr, uaddr ? *uaddr : 0, val);
			return -ENOTSUP;
	}
	return 0;
}
