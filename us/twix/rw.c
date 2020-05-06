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

#include <stdlib.h>
#include <twz/event.h>

static int __select_poll_fd(int fd, uint64_t ev, struct event *event)
{
	struct file *file = twix_get_fd(fd);
	if(!file)
		return -EBADF;
	return twzio_poll(&file->obj, ev, event);
}

long linux_sys_pselect6(int nfds,
  fd_set *readfds,
  fd_set *writefds,
  fd_set *exceptfds,
  const struct timespec *timeout,
  const sigset_t *sigmask)
{
	// twix_log("ENTER SELECT\n");
	long result = 0;
	(void)sigmask;
	bool poll = false;
	if(timeout) {
		if(timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
			poll = true;
		}
		//	twix_log("select timeout: %ld %ld\n", timeout->tv_sec, timeout->tv_nsec);
	}
	struct event *evs = calloc(nfds * 3, sizeof(struct event));
	size_t count = 0, ready = 0;
	while(true) {
		count = 0;
		ready = 0;
		for(int i = 0; i < nfds; i++) {
			//		twix_log("select: %d: %d %d %d\n",
			//		  i,
			//		  readfds ? FD_ISSET(i, readfds) : 0,
			//		  writefds ? FD_ISSET(i, writefds) : 0,
			//		  exceptfds ? FD_ISSET(i, exceptfds) : 0);
			if(exceptfds) {
				FD_CLR(i, exceptfds);
			}
			int ret;
			if(readfds && FD_ISSET(i, readfds)) {
				ret = __select_poll_fd(i, TWZIO_EVENT_READ, &evs[count]);
				//			twix_log("  fdpolling r: %d\n", ret);
				if(ret < 0) {
					result = ret;
					goto done;
				}
				if(timeout) {
					event_add_timeout(&evs[count], timeout);
				}
				count++;
				if(ret > 0) {
					ready += 1;
				}
			}
			if(writefds && FD_ISSET(i, writefds)) {
				ret = __select_poll_fd(i, TWZIO_EVENT_WRITE, &evs[count]);
				//			twix_log("  fdpolling w: %d\n", ret);
				if(ret < 0) {
					result = ret;
					goto done;
				}
				if(timeout) {
					event_add_timeout(&evs[count], timeout);
				}
				count++;
				if(ret > 0) {
					ready += 1;
				}
			}
		}

		if(ready || poll) {
			break;
		}

		//	twix_log("SLEEP SELECT: %ld\n", count);
		//	for(int i = 0; i < count; i++) {
		//		twzobj o = twz_object_from_ptr(evs[i].hdr);
		//		twix_log("  wait on %p %lx: " IDFMT "\n",
		//		  evs[i].hdr,
		//		  evs[i].events,
		//		  IDPR(twz_object_guid(&o)));
		//	}
		event_wait(count, evs);
		//	twix_log("WAKE SELECT\n");
	}

	result = ready;
done:
	free(evs);
	// twix_log("LEAVE SELECT: %ld\n", result);
	return result;
}

long linux_sys_select(int nfds, fd_set *rfds, fd_set *wfds, fd_set *efds, struct timeval *timeout)
{
	struct timespec ts;
	if(timeout) {
		ts.tv_sec = timeout->tv_sec;
		ts.tv_nsec = timeout->tv_usec * 1000;
	}
	return linux_sys_pselect6(nfds, rfds, wfds, efds, timeout ? &ts : NULL, NULL);
}
