#include <errno.h>
#include <fcntl.h>
#include <string.h>
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

#include <twz/fault.h>
#include <twz/twztry.h>
long linux_sys_stat(const char *path, struct stat *sb)
{
	twzobj obj;
	// twix_log(":: stat %s\n", path);
	int r = twz_object_init_name(&obj, path, FE_READ);
	if(r < 0) {
		// twix_log("--- stat %s: no obj: %d\n", path, r);
		return r;
	}
	struct twz_name_ent ent;
	if((r = twz_hier_resolve_name(twz_name_get_root(), path, 0, &ent)) < 0) {
		// twix_log("--- stat %s: no obj2: %d\n", path, r);
		twz_object_release(&obj);
		return r;
	}

	memset(sb, 0, sizeof(*sb));
	switch(ent.type) {
		case NAME_ENT_REGULAR:
			sb->st_mode = S_IFREG;
			break;
		case NAME_ENT_NAMESPACE:
			sb->st_mode = S_IFDIR;
			break;
		case NAME_ENT_SYMLINK:
			sb->st_mode = S_IFLNK;
			break;
	}

	twztry
	{
		struct metainfo *mi = twz_object_meta(&obj);
		sb->st_size = mi->sz;
		sb->st_ino = (uint64_t)twz_object_guid(&obj);
		sb->st_dev = (uint64_t)(twz_object_guid(&obj) >> 64);
		sb->st_mode |= S_IXOTH | S_IROTH | S_IRWXU | S_IRGRP | S_IXGRP;
		twz_object_release(&obj);
	}
	twzcatch(FAULT_OBJECT)
	{
		return -ENOENT;
	}
	twztry_end;
	return 0;
}

long linux_sys_lstat(const char *path, struct stat *sb)
{
	//	twzobj obj;
	struct twz_name_ent ent;
	int r;
	if((r = twz_hier_resolve_name(twz_name_get_root(), path, TWZ_HIER_SYM, &ent)) < 0) {
		// twix_log("--- stat %s: no obj2: %d\n", path, r);
		return r;
	}

	// twix_log(":: lstat %s -> %d\n", path, ent.type);
	if(ent.type != NAME_ENT_SYMLINK) {
		return linux_sys_stat(path, sb);
	}

	/*int r = twz_object_init_guid(&obj, path, FE_READ);
	if(r < 0) {
	    // twix_log("--- stat %s: no obj: %d\n", path, r);
	    return r;
	}*/

	// struct metainfo *mi = twz_object_meta(&obj);
	memset(sb, 0, sizeof(*sb));
	sb->st_size = ent.dlen;
	sb->st_mode = S_IFLNK | S_IWOTH | S_IXOTH | S_IROTH | S_IRWXU | S_IRGRP | S_IXGRP | S_IWGRP;
	//	twz_object_release(&obj);
	return 0;
}

long linux_sys_fstat(int fd, struct stat *sb)
{
	/* TODO: store the dirent with the fd so we can recover the file type */
	struct file *fi = twix_get_fd(fd);
	if(!fi)
		return -EBADF;
	// twix_log("fstat :: %d: " IDFMT "\n", fd, IDPR(twz_object_guid(&fi->obj)));
	int io = 0;
	if(twz_object_getext(&fi->obj, TWZIO_METAEXT_TAG))
		io = 1;

	struct metainfo *mi = twz_object_meta(&fi->obj);
	memset(sb, 0, sizeof(*sb));
	sb->st_size = mi->sz;
	sb->st_ino = (uint64_t)twz_object_guid(&fi->obj);
	sb->st_dev = (uint64_t)(twz_object_guid(&fi->obj) >> 64);
	sb->st_mode = (io ? S_IFIFO : S_IFREG) | S_IXOTH | S_IROTH | S_IRWXU | S_IRGRP | S_IXGRP;
	return 0;
	return -ENOSYS;
}

long linux_sys_faccessat(int dirfd, const char *pathname, int mode, int flags)
{
	// twix_log("ACCESS: (%d) %s: %o\n", dirfd, pathname, mode);
	objid_t id;
	int r = twix_openpathat(dirfd, pathname, flags, &id);
	if(r) {
		return r;
	}

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
	// twix_log("  :: %ld\n", r);
	return r;
}
