#include "syscalls.h"
#include <errno.h>
#include <string.h>
#include <twz/_slots.h>
#include <twz/thread.h>
#include <twz/view.h>

static twzobj unix_obj;
static bool unix_obj_init = false;
static struct unix_repr *uh;

void __linux_init(void)
{
	__fd_sys_init();
	if(!unix_obj_init) {
		unix_obj_init = true;
		uint32_t fl;
		twz_view_get(NULL, TWZSLOT_UNIX, NULL, &fl);
		if(!(fl & VE_VALID)) {
			objid_t id;
			twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &id);
			twz_view_set(NULL, TWZSLOT_UNIX, id, VE_READ | VE_WRITE);
			unix_obj = twz_object_from_ptr(SLOT_TO_VADDR(TWZSLOT_UNIX));

			/* TODO: not sure if this is the right thing... */
			twz_object_wire(NULL, &unix_obj);
			twz_object_delete(&unix_obj, 0);

			uh = twz_object_base(&unix_obj);
			uh->pid = 1;
			uh->tid = 1;
		}

		uh = twz_object_base(&unix_obj);
	}
}

#include <stdarg.h>
#include <stdio.h>
__attribute__((noreturn)) void twix_panic(const char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	vfprintf(stderr, s, ap);
	va_end(ap);
	twz_thread_exit(~0ul);
}

#include <sys/utsname.h>
long linux_sys_uname(struct utsname *u)
{
	strcpy(u->sysname, "Twizzler");
	strcpy(u->nodename, "twizzler"); // TODO
	strcpy(u->release, "0.1");
	strcpy(u->version, "0.1");
	strcpy(u->machine, "x86_64");
	return 0;
}

long linux_sys_getuid(void)
{
	return uh ? uh->uid : -ENOSYS;
}

long linux_sys_getgid(void)
{
	return uh ? uh->gid : -ENOSYS;
}

long linux_sys_geteuid(void)
{
	return uh ? uh->euid : -ENOSYS;
}

long linux_sys_getegid(void)
{
	return uh ? uh->egid : -ENOSYS;
}
