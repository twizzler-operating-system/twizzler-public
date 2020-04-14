#include "syscalls.h"
#include <errno.h>
#include <fcntl.h>
#include <twz/name.h>
#include <twz/view.h>
#include <unistd.h>

#include <twz/debug.h>
long linux_sys_open(const char *path, int flags, int mode)
{
	(void)mode;
	objid_t id;
	int r;

	/* HACK to make libbacktrace work */
	if(!strcmp(path, "/proc/self/exe")) {
		twzobj o0 = twz_object_from_ptr(NULL);
		id = twz_object_guid(&o0);
	} else {
		if((r = twz_name_resolve(NULL, path, NULL, 0, &id))) {
			return -ENOENT;
		}
	}
	struct file *file = twix_alloc_fd();
	if(!file) {
		return -EMFILE;
	}
	file->fl = ((flags & O_ACCMODE) == O_RDONLY)
	             ? VE_READ
	             : 0 | ((flags & O_ACCMODE) == O_WRONLY)
	                 ? VE_WRITE
	                 : 0 | ((flags & O_ACCMODE) == O_RDWR) ? VE_WRITE | VE_READ : 0;

	file->taken = true;
	twz_view_set(NULL, TWZSLOT_FILES_BASE + file->fd, id, file->fl);
	file->obj = twz_object_from_ptr(SLOT_TO_VADDR(TWZSLOT_FILES_BASE + file->fd));

	return file->fd;
}

long linux_sys_lseek(int fd, off_t off, int whence)
{
	struct file *f = twix_get_fd(fd);
	if(!f) {
		return -EBADF;
	}
	switch(whence) {
		case SEEK_SET:
			f->pos = off;
			break;
		case SEEK_CUR:
			f->pos += off;
			break;
		case SEEK_END:
			return -ENOTSUP;
		default:
			return -EINVAL;
	}
	return f->pos;
}

long linux_sys_fcntl(int fd, int cmd, long arg)
{
	(void)fd;
	(void)cmd;
	(void)arg;
	return -ENOTSUP;
	return 0;
}

long linux_sys_close(int fd)
{
	struct file *f = twix_get_fd(fd);
	if(!f) {
		return -EBADF;
	}
	f->taken = false;
	/* TODO: cleanup the file */
	return 0;
}
