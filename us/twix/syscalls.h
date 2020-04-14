#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <twz/_types.h>

#define MAX_FD 1024

__attribute__((noreturn)) void twix_panic(const char *s, ...);
struct iovec;
struct stat;
struct twix_register_frame;
struct rusage;

#include <sys/select.h>

void __linux_init(void);
void __fd_sys_init(void);
struct file *twix_alloc_fd(void);
struct file *twix_get_fd(int fd);
void twix_copy_fds(twzobj *view);
struct file *twix_get_cwd(void);

#include <twz/obj.h>
struct file {
	twzobj obj;
	size_t pos;
	uint32_t fl;
	int fd;
	bool valid;
	bool taken;
	int fcntl_fl;
};

struct unix_repr {
	int pid;
	int uid;
	int gid;
	int euid;
	int egid;
	int pgid;
	int sid;
	int tid;
};

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

long linux_sys_preadv2(int fd, const struct iovec *iov, int iovcnt, ssize_t off, int flags);
long linux_sys_preadv(int fd, const struct iovec *iov, int iovcnt, size_t off);
long linux_sys_readv(int fd, const struct iovec *iov, int iovcnt);
long linux_sys_pwritev2(int fd, const struct iovec *iov, int iovcnt, ssize_t off, int flags);
long linux_sys_pwritev(int fd, const struct iovec *iov, int iovcnt, size_t off);
long linux_sys_writev(int fd, const struct iovec *iov, int iovcnt);
long linux_sys_pread(int fd, void *buf, size_t count, size_t off);
long linux_sys_pwrite(int fd, const void *buf, size_t count, size_t off);
long linux_sys_read(int fd, void *buf, size_t count);
long linux_sys_write(int fd, void *buf, size_t count);
long linux_sys_ioctl(int fd, unsigned long request, unsigned long arg);

long linux_sys_pselect6(int nfds,
  fd_set *readfds,
  fd_set *writefds,
  fd_set *exceptfds,
  const struct timespec *timout,
  const sigset_t *sigmask);

long linux_sys_arch_prctl(int code, unsigned long addr);

long linux_sys_open(const char *path, int flags, int mode);
long linux_sys_close(int fd);
long linux_sys_lseek(int fd, off_t off, int whence);

long linux_sys_access(const char *pathname, int mode);
long linux_sys_faccessat(int dirfd, const char *pathname, int mode, int flags);
long linux_sys_stat(const char *path, struct stat *sb);

long linux_sys_execve(const char *path, const char *const *argv, char *const *env);

long linux_sys_clone(struct twix_register_frame *frame,
  unsigned long flags,
  void *child_stack,
  int *ptid,
  int *ctid,
  unsigned long newtls);
long linux_sys_mmap(void *addr, size_t len, int prot, int flags, int fd, size_t off);
long linux_sys_exit(int code);
long linux_sys_set_tid_address();
long linux_sys_fork(struct twix_register_frame *frame);
long linux_sys_wait4(long pid, int *wstatus, int options, struct rusage *rusage);

long linux_sys_getegid(void);
long linux_sys_geteuid(void);
long linux_sys_getgid(void);
long linux_sys_getuid(void);
struct utsname;
long linux_sys_uname(struct utsname *u);
long linux_sys_fcntl(int fd, int cmd, long arg);
long linux_sys_fstat(int fd, struct stat *sb);
long linux_sys_futex(int *uaddr,
  int op,
  int val,
  const struct timespec *timeout,
  int *uaddr2,
  int val3);
long linux_sys_clock_gettime(clockid_t clock, struct timespec *tp);
