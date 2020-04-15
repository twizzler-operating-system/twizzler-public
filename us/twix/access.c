#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <twz/hier.h>
#include <twz/io.h>
#include <twz/name.h>
#include <twz/obj.h>

#include "syscalls.h"

int twix_openpathat(int dfd, const char *path, int flags, objid_t *id)
{
	(void)flags; // TODO
	int r;
	if(path[0] == '/') {
		r = twz_name_resolve(NULL, path, NULL, 0, id);
		if(r)
			return r;
	} else {
		struct file *df = dfd == AT_FDCWD ? twix_get_cwd() : twix_get_fd(dfd);
		if(!df)
			return -EBADF;
		struct twz_name_ent ent;
		int r = twz_hier_resolve_name(&df->obj, path, 0, &ent);
		if(r)
			return r;

		*id = ent.id;
	}
	return 0;
}

long linux_sys_stat(const char *path, struct stat *sb)
{
	twzobj obj;
	twix_log(":: stat %s\n", path);
	int r = twz_object_init_name(&obj, path, FE_READ);
	if(r < 0) {
		twix_log("--- stat %s: no obj\n", path);
		return r;
	}

	struct metainfo *mi = twz_object_meta(&obj);
	sb->st_size = mi->sz;
	sb->st_mode = S_IFREG | S_IXOTH | S_IROTH | S_IRWXU | S_IRGRP | S_IXGRP;
	return 0;
	return -ENOSYS;
}

long linux_sys_fstat(int fd, struct stat *sb)
{
	struct file *fi = twix_get_fd(fd);
	if(!fi)
		return -EBADF;
	int io = 0;
	if(twz_object_getext(&fi->obj, TWZIO_METAEXT_TAG))
		io = 1;
	struct metainfo *mi = twz_object_meta(&fi->obj);
	sb->st_size = mi->sz;
	sb->st_mode = (io ? S_IFIFO : S_IFREG) | S_IXOTH | S_IROTH | S_IRWXU | S_IRGRP | S_IXGRP;
	return 0;
	return -ENOSYS;
}

long linux_sys_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
	twix_log("ACCESS: (%d) %s\n", dirfd, pathname);
	objid_t id;
	int r = twix_openpathat(dirfd, pathname, flags, &id);

	twzobj obj;
	r = twz_object_init_guid(&obj, id, FE_READ);
	if(r)
		return r;

	struct metainfo *mi = twz_object_meta(&obj);
	if(mode == F_OK)
		return 0;

	if(mode & X_OK) {
		if(!(mi->p_flags & MIP_DFL_EXEC))
			return -EACCES;
	}
	if(mode & R_OK) {
		if(!(mi->p_flags & MIP_DFL_READ))
			return -EACCES;
	}
	if(mode & W_OK) {
		if(!(mi->p_flags & MIP_DFL_WRITE))
			return -EACCES;
	}
	return 0;
}

long linux_sys_access(const char *pathname, int mode)
{
	long r = linux_sys_faccessat(AT_FDCWD, pathname, mode, 0);
	twix_log("  :: %ld\n", r);
	return r;
}
