#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/select.h>
#include <twz/io.h>
#include <twz/obj.h>
#include <twz/view.h>

#include <twz/debug.h>

#include "syscalls.h"

#define DW_NONBLOCK 1

struct iovec {
	void *base;
	size_t len;
};

static ssize_t __do_write(twzobj *o, size_t off, void *base, size_t len, int flags)
{
	/* TODO: more general cases */
	ssize_t r = twzio_write(o, base, len, off, (flags & DW_NONBLOCK) ? TWZIO_NONBLOCK : 0);
	// ssize_t r = bstream_write(o, base, len, 0);
	if(r == -ENOTSUP) {
		// twix_log("__do_write %ld %ld\n", off, len);
		/* TODO: bounds check */
		memcpy((char *)twz_object_base(o) + off, base, len);
		r = len;
		struct metainfo *mi = twz_object_meta(o);
		/* TODO: append */
		if(mi->flags & MIF_SZ) {
			if(off + len > mi->sz) {
				mi->sz = off + len;
			}
		}
	}
	return r;
}

static ssize_t __do_read(twzobj *o, size_t off, void *base, size_t len, int flags)
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
		memcpy(base, (char *)twz_object_base(o) + off, len);
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
	struct file *file = twix_get_fd(fd);
	if(!file)
		return -EBADF;

	if(!(file->fl & VE_READ)) {
		return -EACCES;
	}
	ssize_t count = 0;
	for(int i = 0; i < iovcnt; i++) {
		const struct iovec *v = &iov[i];
		ssize_t r = __do_read(&file->obj,
		  (off < 0) ? file->pos : (size_t)off,
		  v->base,
		  v->len,
		  (count == 0) ? 0 : DW_NONBLOCK);
		if(r >= 0) {
			if(r == 0 && count) {
				break;
			}
			if(off == -1) {
				file->pos += r;
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

	struct file *file = twix_get_fd(fd);
	if(!file)
		return -EBADF;

	if(!(file->fl & VE_WRITE)) {
		return -EACCES;
	}
	ssize_t count = 0;
	for(int i = 0; i < iovcnt; i++) {
		const struct iovec *v = &iov[i];
		ssize_t r = __do_write(&file->obj,
		  (off < 0) ? file->pos : (size_t)off,
		  v->base,
		  v->len,
		  (count == 0) ? 0 : DW_NONBLOCK);
		if(r >= 0) {
			if(r == 0 && count) {
				break;
			}
			if(off == -1) {
				file->pos += r;
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
	struct file *file = twix_get_fd(fd);
	if(!file)
		return -EBADF;
	return twzio_ioctl(&file->obj, request, arg);
}

long linux_sys_pselect6(int nfds,
  fd_set *readfds,
  fd_set *writefds,
  fd_set *exceptfds,
  const struct timespec *timeout,
  const sigset_t *sigmask)
{
	(void)nfds;
	(void)readfds;
	(void)writefds;
	(void)exceptfds;
	(void)timeout;
	(void)sigmask;
	return 0;
}
