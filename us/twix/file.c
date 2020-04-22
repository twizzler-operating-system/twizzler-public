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
			if(!(flags & O_CREAT)) {
				return -ENOENT;
			}
			if((r = twz_object_create(
			      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE, 0, 0, &id))) {
				return r;
			}
			if((r = twz_name_assign(id, path))) {
				twz_object_delete_guid(id, 0);
				return r;
			}

			twzobj no;
			twz_object_init_guid(&no, id, FE_READ | FE_WRITE);
			struct metainfo *mi = twz_object_meta(&no);
			mi->flags |= MIF_SZ;
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

static int internal_dup(int oldfd, int newfd, int flags, int vers)
{
	twix_log("internal dup %d %d %x %d\n", oldfd, newfd, flags, vers);
	if(oldfd == newfd && vers == 3) {
		twix_log("   -> inval\n");
		return -EINVAL;
	} else if(oldfd == newfd && vers == 2) {
		twix_log("   -> noop\n");
		return newfd;
	}
	struct file *nf;
	if(vers >= 2)
		nf = twix_grab_fd(newfd);
	else
		nf = twix_alloc_fd();
	struct file *of = twix_get_fd(oldfd);
	if(!nf || !of) {
		twix_log("   -> err %p %p\n", of, nf);
		if(!nf && vers == 1)
			return -EMFILE;
		return -EBADF;
	}

	if(nf->taken) {
		//	twz_object_release(&nf->obj);
	}
	nf->fl = of->fl;
	nf->fcntl_fl = of->fcntl_fl;
	nf->fd = newfd;
	nf->pos = 0;
	twz_view_set(NULL, TWZSLOT_FILES_BASE + nf->fd, twz_object_guid(&of->obj), nf->fl);
	nf->obj = twz_object_from_ptr(SLOT_TO_VADDR(TWZSLOT_FILES_BASE + nf->fd));
	nf->taken = nf->valid = true;
	return newfd;
}

int linux_sys_dup3(int oldfd, int newfd, int flags)
{
	return internal_dup(oldfd, newfd, flags, 3);
}

int linux_sys_dup2(int oldfd, int newfd)
{
	return internal_dup(oldfd, newfd, 0, 2);
}

int linux_sys_dup(int oldfd)
{
	return internal_dup(oldfd, -1, 0, 1);
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
