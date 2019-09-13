#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* TODO: arch-dep */

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
	uint64_t return_addr;
};

#include <errno.h>
#include <twz/_slots.h>
#include <twz/_types.h>
#include <twz/_view.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/view.h>

#include <twz/debug.h>

#define LINUX_SYS_read 0
#define LINUX_SYS_write 1
#define LINUX_SYS_open 2
#define LINUX_SYS_close 3
#define LINUX_SYS_stat 4

#define LINUX_SYS_mmap 9

#define LINUX_SYS_ioctl 16
#define LINUX_SYS_pread 17
#define LINUX_SYS_pwrite 18
#define LINUX_SYS_readv 19
#define LINUX_SYS_writev 20

#define LINUX_SYS_clone 56
#define LINUX_SYS_fork 57
#define LINUX_SYS_execve 59
#define LINUX_SYS_exit 60
#define LINUX_SYS_wait4 61

#define LINUX_SYS_arch_prctl 158

#define LINUX_SYS_set_tid_address 218

#define LINUX_SYS_exit_group 231

#define LINUX_SYS_pselect6 270

#define LINUX_SYS_preadv 295
#define LINUX_SYS_pwritev 296

#define LINUX_SYS_preadv2 327
#define LINUX_SYS_pwritev2 328

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

long linux_sys_arch_prctl(int code, unsigned long addr)
{
	switch(code) {
		case ARCH_SET_FS:
			sys_thrd_ctl(THRD_CTL_SET_FS, (long)addr);
			break;
		case ARCH_SET_GS:
			sys_thrd_ctl(THRD_CTL_SET_GS, (long)addr);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

struct iovec {
	void *base;
	size_t len;
};

struct file {
	struct object obj;
	size_t pos;
	uint32_t fl;
	int fd;
	bool valid;
	bool taken;
	int fcntl_fl;
};

#include <twz/thread.h>

struct process {
	struct thread thrd;
	int pid;
};

#define MAX_PID 1024
static struct process pds[MAX_PID];

#define MAX_FD 1024
static struct file fds[MAX_FD];

#define DW_NONBLOCK 1

#include <fcntl.h>

static int __check_fd_valid(int fd)
{
	if(!fds[fd].valid) {
		twz_view_get(NULL, TWZSLOT_FILES_BASE + fd, NULL, &fds[fd].fl);
		fds[fd].valid = true;
		if(!(fds[fd].fl & VE_VALID)) {
			return -EBADF;
		}
		fds[fd].obj = TWZ_OBJECT_INIT(TWZSLOT_FILES_BASE + fd);
		fds[fd].taken = true;
	} else if(!fds[fd].taken) {
		return -EBADF;
	}
	return 0;
}

static void __fd_sys_init(void)
{
	static bool fds_init = false;
	if(fds_init) {
		return;
	}
	fds_init = true;
	for(size_t i = 0; i < MAX_FD; i++)
		__check_fd_valid(i);
}

static int __alloc_fd(void)
{
	for(int i = 0; i < MAX_FD; i++) {
		int test = __check_fd_valid(i);
		if(test < 0)
			return i;
	}
	return -EMFILE;
}

#include <sys/stat.h>
long linux_sys_stat(const char *path, struct stat *sb)
{
	struct object obj;
	int r = twz_object_open_name(&obj, path, FE_READ);
	if(r < 0) {
		debug_printf("--- stat %s: no obj\n", path);
		return r;
	}

	struct metainfo *mi = twz_object_meta(&obj);
	sb->st_size = mi->sz;
	sb->st_mode = S_IFREG | S_IXOTH | S_IROTH | S_IRWXU | S_IRGRP | S_IXGRP;
	return 0;
	return -ENOSYS;
}

long linux_sys_open(const char *path, int flags, int mode)
{
	objid_t id;
	int r;
	if((r = twz_name_resolve(NULL, path, NULL, 0, &id))) {
		return -ENOENT;
	}
	int fd = __alloc_fd();
	if(fd < 0) {
		return fd;
	}
	struct file *file = &fds[fd];
	file->fl =
	  (flags == O_RDONLY)
	    ? VE_READ
	    : 0 | (flags == O_WRONLY) ? VE_WRITE : 0 | (flags == O_RDWR) ? VE_WRITE | VE_READ : 0;

	file->taken = true;
	twz_view_set(NULL, TWZSLOT_FILES_BASE + fd, id, file->fl);
	file->obj = TWZ_OBJECT_INIT(TWZSLOT_FILES_BASE + fd);

	return fd;
}

long linux_sys_close(int fd)
{
	int test = __check_fd_valid(fd);
	if(test < 0) {
		return test;
	}
	fds[fd].taken = false;
	return 0;
}

#include <twz/thread.h>
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
long linux_sys_execve(const char *path, char **argv, char *const *env)
{
	objid_t id = 0;
	int r = twz_name_resolve(NULL, path, NULL, 0, &id);
	if(r) {
		return r;
	}

	objid_t vid;
	struct object view;
	if((r = twz_exec_create_view(&view, id, &vid)) < 0) {
		return r;
	}

	for(size_t i = 0; i < MAX_FD; i++) {
		struct file *f = &fds[i];
		/* TODO: CLOEXEC */
		if(f->valid && f->taken) {
			objid_t fi;
			uint32_t fl;
			twz_view_get(NULL, TWZSLOT_FILES_BASE + i, &fi, &fl);
			twz_view_set(&view, TWZSLOT_FILES_BASE + i, fi, fl);
		}
	}
	struct object exe;
	twz_object_open(&exe, id, FE_READ);
	struct elf64_header *hdr = twz_obj_base(&exe);

	return twz_exec_view(&view, vid, hdr->e_entry, argv, env);
}

#include <twz/bstream.h>
#include <twz/io.h>
static ssize_t __do_write(struct object *o, size_t off, void *base, size_t len, int flags)
{
	/* TODO: more general cases */
	ssize_t r = twzio_write(o, base, len, off, (flags & DW_NONBLOCK) ? TWZIO_NONBLOCK : 0);
	// ssize_t r = bstream_write(o, base, len, 0);
	if(r == -ENOTSUP) {
		/* TODO: bounds check */
		memcpy((char *)twz_obj_base(o) + off, base, len);
		r = len;
		struct metainfo *mi = twz_object_meta(o);
		/* TODO: append */
		if(mi->flags & mi->sz) {
			if(off + len > mi->sz) {
				mi->sz = off + len;
			}
		}
	}
	return r;
}

static ssize_t __do_read(struct object *o, size_t off, void *base, size_t len, int flags)
{
	ssize_t r = twzio_read(o, base, len, off, (flags & DW_NONBLOCK) ? TWZIO_NONBLOCK : 0);
	if(r == -ENOTSUP) {
		struct metainfo *mi = twz_object_meta(o);
		if(mi->flags & MIF_SZ) {
			if(off >= mi->sz) {
				return 0;
			}
			if(off + len > mi->sz) {
				len = mi->sz - off;
			}
		} else {
			return -EIO;
		}
		memcpy(base, (char *)twz_obj_base(o) + off, len);
		r = len;
	}
	return r;
}

long linux_sys_preadv2(int fd, const struct iovec *iov, int iovcnt, ssize_t off, int flags)
{
	(void)flags; /* TODO (minor): implement */
	if(fd < 0 || fd >= MAX_FD) {
		return -EINVAL;
	}
	int test = __check_fd_valid(fd);
	if(test < 0)
		return test;

	if(!(fds[fd].fl & VE_READ)) {
		return -EACCES;
	}
	ssize_t count = 0;
	for(int i = 0; i < iovcnt; i++) {
		const struct iovec *v = &iov[i];
		ssize_t r = __do_read(&fds[fd].obj,
		  (off < 0) ? fds[fd].pos : (size_t)off,
		  v->base,
		  v->len,
		  (count == 0) ? 0 : DW_NONBLOCK);
		if(r >= 0) {
			if(r == 0 && count) {
				break;
			}
			if(off == -1) {
				fds[fd].pos += r;
			} else {
				off += r;
			}
			count += r;
		} else {
			return count ? count : r;
		}
	}
	return count;
}

long linux_sys_preadv(int fd, const struct iovec *iov, int iovcnt, size_t off)
{
	return linux_sys_preadv2(fd, iov, iovcnt, off, 0);
}

long linux_sys_readv(int fd, const struct iovec *iov, int iovcnt)
{
	return linux_sys_preadv2(fd, iov, iovcnt, -1, 0);
}

long linux_sys_pwritev2(int fd, const struct iovec *iov, int iovcnt, ssize_t off, int flags)
{
	(void)flags; /* TODO (minor): implement */
	if(fd < 0 || fd >= MAX_FD) {
		return -EINVAL;
	}

	int test = __check_fd_valid(fd);
	if(test < 0)
		return test;

	if(!(fds[fd].fl & VE_WRITE)) {
		return -EACCES;
	}
	ssize_t count = 0;
	for(int i = 0; i < iovcnt; i++) {
		const struct iovec *v = &iov[i];
		ssize_t r = __do_write(&fds[fd].obj,
		  (off < 0) ? fds[fd].pos : (size_t)off,
		  v->base,
		  v->len,
		  (count == 0) ? 0 : DW_NONBLOCK);
		if(r >= 0) {
			if(r == 0 && count) {
				break;
			}
			if(off == -1) {
				fds[fd].pos += r;
			} else {
				off += r;
			}
			count += r;
		} else {
			break;
		}
	}
	return count;
}

long linux_sys_pwritev(int fd, const struct iovec *iov, int iovcnt, size_t off)
{
	return linux_sys_pwritev2(fd, iov, iovcnt, off, 0);
}

long linux_sys_writev(int fd, const struct iovec *iov, int iovcnt)
{
	return linux_sys_pwritev2(fd, iov, iovcnt, -1, 0);
}

long linux_sys_pread(int fd, void *buf, size_t count, size_t off)
{
	struct iovec v = { .base = buf, .len = count };
	return linux_sys_preadv(fd, &v, 1, off);
}

long linux_sys_pwrite(int fd, const void *buf, size_t count, size_t off)
{
	struct iovec v = { .base = (void *)buf, .len = count };
	return linux_sys_pwritev(fd, &v, 1, off);
}

long linux_sys_read(int fd, void *buf, size_t count)
{
	struct iovec v = { .base = buf, .len = count };
	return linux_sys_preadv(fd, &v, 1, -1);
}

long linux_sys_write(int fd, void *buf, size_t count)
{
	struct iovec v = { .base = (void *)buf, .len = count };
	return linux_sys_pwritev(fd, &v, 1, -1);
}

long linux_sys_ioctl(int fd, unsigned long request, unsigned long arg)
{
	debug_printf("IOCTL: %d %ld %ld\n", fd, request, arg);
	return 0;
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
	if(flags != 0x7d0f00) {
		return -ENOSYS;
	}

	memcpy((void *)((uintptr_t)child_stack - sizeof(struct twix_register_frame)),
	  frame,
	  sizeof(struct twix_register_frame));
	child_stack = (void *)((uintptr_t)child_stack - sizeof(struct twix_register_frame));
	struct thread thr;
	int r;
	if((r = twz_thread_spawn(&thr,
	      &(struct thrd_spawn_args){ .start_func = __return_from_clone,
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
long linux_sys_mmap(void *addr, size_t len, int prot, int flags, int fd, size_t off)
{
	if(addr != NULL && (flags & MAP_FIXED)) {
		return -ENOTSUP;
	}
	if(fd >= 0) {
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
		twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &nid);
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
	/* TODO: code */
	twz_thread_exit();
	return 0;
}

long linux_sys_set_tid_address()
{
	/* TODO: NI */
	return 0;
}

#include <sys/select.h>
#include <sys/signal.h>
long linux_sys_pselect6(int nfds,
  fd_set *readfds,
  fd_set *writefds,
  fd_set *exceptfds,
  const struct timespec *timout,
  const sigset_t *sigmask)
{
	return 0;
}

long linux_sys_fork(struct twix_register_frame *frame)
{
	int r;
	objid_t vid;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &vid))) {
		return r;
	}

	struct object view;
	if((r = twz_object_open(&view, vid, FE_READ | FE_WRITE))) {
		return r;
	}

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
	twz_thread_create(&pds[pid].thrd);

	struct twzthread_repr *newrepr = twz_obj_base(&pds[pid].thrd.obj);

	objid_t sid;
	for(size_t i = 0; i <= TWZSLOT_MAX_SLOT; i++) {
		objid_t id;
		uint32_t flags;
		twz_view_get(NULL, i, &id, &flags);
		if(!(flags & VE_VALID)) {
			continue;
		}
		if(i == TWZSLOT_THRD) {
			if(flags & VE_FIXED)
				twz_view_fixedset(&pds[pid].thrd.obj, i, pds[pid].thrd.tid, flags);
			else
				twz_view_set(&view, i, pds[pid].thrd.tid, flags);
		} else if(i == TWZSLOT_CVIEW) {
			twz_view_set(&view, i, vid, VE_READ | VE_WRITE);
		} else if(i >= TWZSLOT_FILES_BASE
		          || !(flags & TWZ_OC_DFL_WRITE) /* TODO: this probably isn't safe */) {
			/* Copy directly */
			twz_view_set(&view, i, id, flags);
		} else {
			/* Copy-derive */
			objid_t nid;
			if((r = twz_object_create(
			      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_DFL_EXEC /* TODO */, 0, id, &nid))) {
				/* TODO: cleanup */
				return r;
			}
			if(flags & VE_FIXED)
				twz_view_fixedset(&pds[pid].thrd.obj, i, nid, flags);
			else
				twz_view_set(&view, i, nid, flags);
			if(i == TWZSLOT_STACK)
				sid = nid;
		}
	}

	if(!sid) /* TODO */
		return -EIO;

	struct object stack;
	twz_object_open(&stack, sid, FE_READ | FE_WRITE);

	size_t soff = (uint64_t)twz_ptr_local(frame) - 1024;
	void *childstack = twz_ptr_lea(&stack, (void *)soff);

	memcpy(childstack, frame, sizeof(struct twix_register_frame));

	uint64_t fs;
	asm volatile("rdfsbase %%rax" : "=a"(fs));

	struct sys_thrd_spawn_args sa = {
		.target_view = vid,
		.start_func = (void (*)(void *))__return_from_fork,
		.arg = NULL,
		.stack_base = (void *)twz_ptr_rebase(TWZSLOT_STACK, soff),
		.stack_size = 8,
		.tls_base = (void *)fs,
		.thrd_ctrl = TWZSLOT_THRD,
	};

	if((r = sys_thrd_spawn(pds[pid].thrd.tid, &sa, 0))) {
		return r;
	}

	return pid;
}

struct rusage;
long linux_sys_wait4(long pid, int *wstatus, int options, struct rusage *rusage)
{
	// debug_printf("WAIT: %ld %p %x\n", pid, wstatus, options);

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

		for(unsigned int i = 0; i < c; i++) {
			if(event[i] && pds[pids[i]].pid) {
				if(wstatus) {
					*wstatus = 0; // TODO
				}
				pds[pids[i]].pid = 0;
				return pids[i];
			}
		}
	}
}

static long (*syscall_table[])() = {
	[LINUX_SYS_arch_prctl] = linux_sys_arch_prctl,
	[LINUX_SYS_set_tid_address] = linux_sys_set_tid_address,
	[LINUX_SYS_pwritev2] = linux_sys_pwritev2,
	[LINUX_SYS_pwritev] = linux_sys_pwritev,
	[LINUX_SYS_writev] = linux_sys_writev,
	[LINUX_SYS_preadv2] = linux_sys_preadv2,
	[LINUX_SYS_preadv] = linux_sys_preadv,
	[LINUX_SYS_readv] = linux_sys_readv,
	[LINUX_SYS_pread] = linux_sys_pread,
	[LINUX_SYS_pwrite] = linux_sys_pwrite,
	[LINUX_SYS_read] = linux_sys_read,
	[LINUX_SYS_write] = linux_sys_write,
	[LINUX_SYS_ioctl] = linux_sys_ioctl,
	[LINUX_SYS_exit] = linux_sys_exit,
	[LINUX_SYS_exit_group] = linux_sys_exit,
	[LINUX_SYS_open] = linux_sys_open,
	[LINUX_SYS_close] = linux_sys_close,
	[LINUX_SYS_mmap] = linux_sys_mmap,
	[LINUX_SYS_execve] = linux_sys_execve,
	[LINUX_SYS_clone] = linux_sys_clone,
	[LINUX_SYS_pselect6] = linux_sys_pselect6,
	[LINUX_SYS_fork] = linux_sys_fork,
	[LINUX_SYS_wait4] = linux_sys_wait4,
	[LINUX_SYS_stat] = linux_sys_stat,
};

static size_t stlen = sizeof(syscall_table) / sizeof(syscall_table[0]);

long twix_syscall(long num, long a0, long a1, long a2, long a3, long a4, long a5)
{
	__fd_sys_init();
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
#if 1
		debug_printf("Unimplemented Linux system call: %ld\n", num);
#endif
		return -ENOSYS;
	}
	if(num == LINUX_SYS_clone) {
		/* needs frame */
		return -ENOSYS;
	}
	if(num == LINUX_SYS_fork) {
		/* needs frame */
		return -ENOSYS;
	}
	long r = syscall_table[num](a0, a1, a2, a3, a4, a5);
	// debug_printf("sc %ld ret %ld\n", num, r);
	return r;
}

static long twix_syscall_frame(struct twix_register_frame *frame,
  long num,
  long a0,
  long a1,
  long a2,
  long a3,
  long a4,
  long a5)
{
	__fd_sys_init();
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
#if 1
		debug_printf("Unimplemented Linux system call: %ld\n", num);
#endif
		return -ENOSYS;
	}
	if(num == LINUX_SYS_clone) {
		/* needs frame */
		return syscall_table[num](frame, a0, a1, a2, a3, a4, a5);
	} else if(num == LINUX_SYS_fork) {
		/* needs frame */
		return syscall_table[num](frame, a0, a1, a2, a3, a4, a5);
	}
	long r = syscall_table[num](a0, a1, a2, a3, a4, a5);
	// debug_printf("sc %ld ret %ld\n", num, r);
	return r;
}

long __twix_syscall_target_c(long num, struct twix_register_frame *frame)
{
	long ret = twix_syscall_frame(
	  frame, num, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
	return ret;
}

asm(".global __twix_syscall_target;"
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

    "movabs $__twix_syscall_target_c, %rcx;"
    "call *%rcx;"

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
    "ret;");
