#include <debug.h>

#include <twzkv.h>
#include <twzobj.h>
#include <twzthread.h>
#include <twzname.h>

static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("mfence; rdtscp" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

static size_t readl(char *data, size_t dlen, char *buf, size_t blen)
{
	if(dlen == 0 || blen == 0) return 0;
	char *nl = strchr(data, '\n');
	if(!nl) nl = data + dlen;
	size_t len = nl - data;
	if(len > blen) len = blen;
	strncpy(buf, data, len);

	return len;
}

void name_prepare(void)
{
	struct object kc;
	if(twz_object_open(&kc, 1, FE_READ) < 0) {
		return;
	}

	char *kcdata = twz_ptr_base(&kc);
	size_t kclen = strlen(kcdata);
	kc.mi->flags |= MIF_SZ;
	kc.mi->sz = kclen;
	char *r = strstr(kcdata, "name=");
	if(!r) {
		return;
	}

	char *idstr = r + 5;
	objid_t id = 0;
	if(!twz_objid_parse(idstr, &id)) {
		return;
	}

	struct object name_index;
	twz_object_open(&name_index, id, FE_READ | FE_WRITE);
	char *_namedata = twz_ptr_base(&name_index);
	char namedata[strlen(_namedata)+1];
	memcpy(namedata, _namedata, sizeof(namedata));
	memset(_namedata, 0, sizeof(namedata));

	twzkv_init_index(&name_index);

	size_t len = strlen(namedata);
	char *tmp = namedata;
	char buf[128];
	size_t tl;
	while((tl=readl(tmp, len, buf, sizeof(buf))) > 0) {
		if(tl == sizeof(buf)) {
			debug_printf("Increase buffer size");
			twz_thread_exit();
		}
		char *e = strchr(buf, '=');
		if(e) {
			*e++ = 0;
			objid_t id;
			if(twz_objid_parse(e, &id)) {
				debug_printf("Found name %s -> " IDFMT, buf, IDPR(id));
				twz_name_assign(id, buf, NAME_RESOLVER_DEFAULT);
			}
		}

		tl++;
		len -= tl;
		tmp += tl;
	}
	twz_name_assign(1, "__kc", NAME_RESOLVER_DEFAULT);

}

#include <twzexec.h>

static void _thrd_exec(void *arg)
{
	objid_t *ei = __twz_ptr_lea(&stdobj_thrd, arg);
	twz_exec(*ei, 0);
}

#include <stdio.h>

#include <unistd.h>


void map_io(const char *name, size_t slot)
{
	objid_t id = twz_name_resolve(NULL, name, NAME_RESOLVER_DEFAULT);
	if(id == 0) {
		debug_printf("Failed to map %s", name);
		return;
	}
	twz_view_set(NULL, slot, id, VE_READ | VE_WRITE);
}

int main()
{
	debug_printf("init - starting\n");

	name_prepare();

	objid_t sid = twz_name_resolve(NULL, "shell/shell.0", NAME_RESOLVER_DEFAULT);
	debug_printf("SHELL: " IDFMT, IDPR(sid));

	ssize_t r;
	/*
	objid_t oid;
	twz_object_new(NULL, &oid, 0, 0, 0);
	twz_view_set(NULL, TWZSLOT_FILES_BASE + 1, oid, VE_READ | VE_WRITE);

	ssize_t r = pwrite(1, "Hello!", 6, 0);
	debug_printf("pwrite -> %ld\n", r);
	char buf[8] = {0};
	r = pread(1, buf, 6, 0);
	debug_printf("pread -> %ld: %s\n", r, buf);

	*/


	objid_t termid = twz_name_resolve(NULL, "term/term.0", NAME_RESOLVER_DEFAULT);
	if(termid) {
		struct twzthread termthrd;
		if(twz_thread_spawn(&termthrd, _thrd_exec, NULL, &termid, 0) < 0) {
			debug_printf("Failed to spawn term thread");
			return 1;
		}
		debug_printf("WAITING FOR READY");
		twz_thread_wait_ready(&termthrd);
		debug_printf("WAITING FOR READY: ok");
	} else {
		debug_printf("Failed to spawn term");
		return 1;
	}


	map_io("keyboard", TWZSLOT_STDIN);
	map_io("screen",   TWZSLOT_STDOUT);
	map_io("screen",   TWZSLOT_STDERR);

	debug_printf("Sending test string");
	printf("Testing!\n");
	debug_printf("EOL");

	objid_t shellid = twz_name_resolve(NULL, "shell/shell.0", NAME_RESOLVER_DEFAULT);
	if(termid) {
		struct twzthread shellthrd;
		if(twz_thread_spawn(&shellthrd, _thrd_exec, NULL, &shellid, 0) < 0) {
			debug_printf("Failed to spawn shell thread");
			return 1;
		}
	} else {
		debug_printf("Failed to spawn shell");
		return 1;
	}


	twz_thread_exit();
	for(;;);
	struct twzkv_item k = {
		.data = "foo",
		.length = 3,
	};

	struct twzkv_item v = {
		.data = "bar",
		.length = 3,
	};

	struct twzkv_item rv;

	struct object index;
	struct object data;
	if(twzkv_create_index(&index)) {
		debug_printf("Failed to create index");
		return 1;
	}
	if(twzkv_create_data(&data)) {
		debug_printf("Failed to create data");
		return 1;
	}

	r = twzkv_put(&index, &data, &k, &v);
	debug_printf("put: %d\n", r);
	uint64_t start = rdtsc();
	int i;
	for(i=0;i<10000000;i++) {
		r = twzkv_get(&index, &k, &rv);
	}
	uint64_t end = rdtsc();
	debug_printf("(%ld, %ld) diff: %ld (%ld per)\n", end, start, end - start, (end - start) / i);

	debug_printf(":: %s\n", rv.data);
	

	for(;;);
}

