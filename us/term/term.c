#include <twzthread.h>
#include <bstream.h>
#include <twzobj.h>
#include <twzname.h>

#include <debug.h>

static void _input_thrd(void *arg)
{
	debug_printf("term input - starting");
	//sys_thrd_ctl(THRD_CTL_SET_IOPL, 3);
	for(;;) {
	
	}
}

int main()
{
	debug_printf("term - starting");

	struct object ko;
	objid_t koid;
	twz_object_new(&ko, &koid, 0, 0, 0);
	if(bstream_init(&ko, 8)) {
		debug_printf("Failed to create ko");
	}

	struct object so;
	objid_t soid;
	twz_object_new(&so, &soid, 0, 0, 0);
	if(bstream_init(&so, 8)) {
		debug_printf("Failed to create so");
	}

	twz_name_assign(koid, "keyboard", NAME_RESOLVER_DEFAULT);
	twz_name_assign(soid, "screen", NAME_RESOLVER_DEFAULT);

	struct twzthread it;
	if(twz_thread_spawn(&it, _input_thrd, NULL, NULL, 0) < 0) {
		debug_printf("Failed to spawn input thread");
		return 1;
	}

	twz_thread_ready();
	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		bstream_read(&so, buf, 127, 0);
		debug_printf("SOREAD: %s\n", buf);
	}
}

