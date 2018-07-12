#define _INTERNAL_SYSCALL_H
#include "stdio_impl.h"
#include <sys/ioctl.h>
#include <sys/uio.h>

#include <twzobj.h>
#include <twzio.h>
#include <twzslots.h>

#if 0
size_t __stdio_write(FILE *f, const unsigned char *buf, size_t len)
{
	struct object so;
	twz_object_init(&so, TWZSLOT_FILES_BASE + f->fd);
	struct iovec iovs[2] = {
		{ .iov_base = f->wbase, .iov_len = f->wpos-f->wbase },
		{ .iov_base = (void *)buf, .iov_len = len }
	};
	struct iovec *iov = iovs;
	size_t rem = iov[0].iov_len + iov[1].iov_len;
	int iovcnt = 2;
	ssize_t cnt;
	if(iov[0].iov_len == 0) {
		iov++; iovcnt--;
	}
	for (;;) {
		cnt = twzio_write(&so, iov[0].iov_base, iov[0].iov_len, 0);
		if ((size_t)cnt == rem) {
			f->wend = f->buf + f->buf_size;
			f->wpos = f->wbase = f->buf;
			return len;
		}
		if (cnt < 0) {
			f->wpos = f->wbase = f->wend = 0;
			f->flags |= F_ERR;
			return iovcnt == 2 ? 0 : len-iov[0].iov_len;
		}
		rem -= cnt;
		if (cnt > (ssize_t)iov[0].iov_len) {
			cnt -= iov[0].iov_len;
			iov++; iovcnt--;
		}
		iov[0].iov_base = (char *)iov[0].iov_base + cnt;
		iov[0].iov_len -= cnt;
	}
}

size_t __stdio_read(FILE *f, unsigned char *buf, size_t len)
{
	struct object in;
	twz_object_init(&in, TWZSLOT_FILES_BASE + f->fd);
	struct iovec iov[2] = {
		{ .iov_base = buf, .iov_len = len - !!f->buf_size },
		{ .iov_base = f->buf, .iov_len = f->buf_size }
	};
	ssize_t cnt = 0;

	cnt = twzio_read(&in, iov[0].iov_base, iov[0].iov_len, 0);
	if(cnt == iov[0].iov_len) {
		ssize_t rc = twzio_read(&in, iov[1].iov_base, iov[1].iov_len, 0);
		if(rc > 0) cnt += rc;
	}
	if (cnt <= 0) {
		cnt = cnt == 0 ? 0 : -1;
		f->flags |= F_EOF ^ ((F_ERR^F_EOF) & cnt);
		return cnt;
	}
	if (cnt <= iov[0].iov_len) return cnt;
	cnt -= iov[0].iov_len;
	f->rpos = f->buf;
	f->rend = f->buf + cnt;
	if (f->buf_size) buf[len-1] = *f->rpos++;
	return len;
}
#endif
