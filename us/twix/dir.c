#include "syscalls.h"
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <twz/hier.h>
#include <twz/name.h>

ssize_t linux_sys_readlink(const char *path, char *buf, size_t bufsz)
{
	twzobj *root = twz_name_get_root();
	return twz_hier_readlink(root, path, buf, bufsz);
}

struct linux_dirent64 {
	uint64_t d_ino;          /* 64-bit inode number */
	uint64_t d_off;          /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char d_type;    /* File type */
	char d_name[];           /* Filename (null-terminated) */
};

static struct linux_dirent64 *__copy_to_dirp(struct linux_dirent64 *dirp,
  struct twz_name_ent *ent,
  ssize_t avail,
  size_t pos_next,
  ssize_t *sz)
{
	*sz = (sizeof(*dirp) + strlen(ent->name) + 1 + 15) & ~15;
	if(*sz > avail) {
		return NULL;
	}
	dirp->d_off = pos_next;
	dirp->d_ino = 1;
	dirp->d_reclen = *sz;
	switch(ent->type) {
		case NAME_ENT_NAMESPACE:
			dirp->d_type = DT_DIR;
			break;
		case NAME_ENT_REGULAR:
			dirp->d_type = DT_REG;
			break;
		default:
			dirp->d_type = DT_UNKNOWN;
			break;
	}
	strcpy(dirp->d_name, ent->name);
	return (void *)((char *)dirp + *sz);
}

long linux_sys_getdents64(unsigned fd, struct linux_dirent64 *dirp, unsigned int count)
{
	// twix_log("getdents\n");
	if(!dirp)
		return -EINVAL;
	struct file *file = twix_get_fd(fd);
	if(!file) {
		return -EBADF;
	}

	// twix_log("Got here!\n");
	size_t bytes = 0;
	while(true) {
		// twix_log("%ld bytes read, %d remain\n", bytes, count);
		struct twz_name_ent *ent;
		ssize_t sz_h;
		sz_h = twz_hier_get_entry(&file->obj, file->pos, &ent);
		// twix_log("got entry: %ld: %s\n", sz_h, ent->name);
		if(sz_h == 0) {
			//	twix_log("done_h\n");
			return bytes;
		} else if(sz_h < 0) {
			return sz_h;
		}

		if(ent->flags & NAME_ENT_VALID) {
			ssize_t sz_l;
			dirp = __copy_to_dirp(dirp, ent, count, file->pos + sz_h, &sz_l);
			//	twix_log("copy: %ld\n", sz_l);
			if(!dirp) {
				//		twix_log("done_l\n");
				return bytes;
			}
			bytes += sz_l;
			count -= sz_l;
		}
		file->pos += sz_h;
	}
}
