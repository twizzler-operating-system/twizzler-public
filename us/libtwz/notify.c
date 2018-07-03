#include <notify.h>
#include <limits.h>
#include <twzviewcall.h>
#define NOTIFY_METAHEADER (struct metaheader){.id = NOTIFY_HEADER_ID, .len = sizeof(struct notify_header) }

int notify_init(struct object *obj, struct notify_svar *start,
		int count, void (*prep)(struct object *))
{
	struct notify_header *nh = twz_object_addmeta(obj, NOTIFY_METAHEADER);
	if(!nh) {
		return -TE_NOSPC;
	}
	nh->vars = start;
	nh->count = count;
	nh->prepare = prep;
	return 0;
}

int notify_wake(struct object *obj, int n, int count)
{
	struct notify_header *hdr = twz_object_findmeta(obj, NOTIFY_HEADER_ID);
	struct notify_svar *sv = twz_ptr_lea(obj, hdr->vars);
	if(sv[n].flags & NH_VALID) {
		sys_thread_sync(THREAD_SYNC_WAKE, twz_ptr_lea(obj, sv[n].var), count, NULL);
	} else {
		return -TE_NOENTRY;
	}
	return 0;
}

void notify_prepare(struct object *obj)
{
	struct notify_header *hdr = twz_object_findmeta(obj, NOTIFY_HEADER_ID);
	if(hdr && hdr->prepare) {
		void (*_prepare)(struct object *) = twz_ptr_lea(obj, hdr->prepare);
		_prepare(obj);
	}
}

/* TODO: do we need to lock for these operations? */
int notify_wake_all(struct object *obj, int count, uint32_t flags)
{
	struct notify_header *hdr = twz_object_findmeta(obj, NOTIFY_HEADER_ID);
	struct notify_svar *sv = twz_ptr_lea(obj, hdr->vars);
	for(int i=0;i<hdr->count && count > 0;i++) {
		if(sv[i].flags & NH_VALID) {
	//		uint64_t exp = 0;
			if(!(sv[i].flags & NH_REG)) continue;
			if(!(NTYPE(sv[i].flags) & NTYPE(flags))) continue;
			_Atomic uint64_t *ptr = (_Atomic uint64_t *)twz_ptr_lea(obj, sv[i].var);
			if(!(atomic_fetch_or(ptr, 1 << sv[i].id) & (1 << sv[i].id))) {
				sys_thread_sync(THREAD_SYNC_WAKE, ptr, INT_MAX, NULL);
				count--;
			}
//			uint64_t val = (uint64_t)sv[i].id | ((uint64_t)sv[i].flags) << 32;
//			if(atomic_compare_exchange_strong(ptr, &exp, val)) {
//				fbsd_sys_umtx(ptr, UMTX_OP_WAKE, INT_MAX);
//				count--;
//			}
		}
	}
	return 0;
}

int notify_insert(struct object *obj, uint32_t *addr)
{
	struct notify_header *hdr = twz_object_findmeta(obj, NOTIFY_HEADER_ID);
	struct notify_svar *sv = twz_ptr_lea(obj, hdr->vars);
	for(int i=0;i<hdr->count;i++) {
		if(!(atomic_fetch_or(&sv[i].flags, NH_USED) & NH_USED)) {
			sv[i].var = twz_ptr_canon(obj, addr, FE_READ | FE_WRITE);
			atomic_fetch_or(&sv[i].flags, NH_VALID);
			return i;
		}
	}
	return -TE_NOSPC;
}

int notify_register(struct object *obj, _Atomic uint32_t *wait,
		uint32_t id, uint32_t flags)
{
	struct notify_header *hdr = twz_object_findmeta(obj, NOTIFY_HEADER_ID);
	struct notify_svar *sv = twz_ptr_lea(obj, hdr->vars);
	for(int i=0;i<hdr->count;i++) {
		if(!(atomic_fetch_or(&sv[i].flags, NH_USED) & NH_USED)) {
			sv[i].var = (uint64_t *)twz_ptr_canon(obj, wait, FE_READ | FE_WRITE);
			sv[i].flags |= (flags & ~NH_VALID) | NH_REG;
			sv[i].id = id;
			atomic_fetch_or(&sv[i].flags, NH_VALID);
			return i;
		}
	}
	return -TE_NOSPC;
}



int notify_wait(struct object *obj, int n, uint32_t val)
{
	struct notify_header *hdr = twz_object_findmeta(obj, NOTIFY_HEADER_ID);
	struct notify_svar *sv = twz_ptr_lea(obj, hdr->vars);
	if(n >= hdr->count) return -TE_INVALID;
	if(!(sv[n].flags & NH_VALID)) {
		return -TE_NOENTRY;
	}

	/* TODO: check return value */
	sys_thread_sync(THREAD_SYNC_SLEEP, twz_ptr_lea(obj, sv[n].var), val, NULL);
	return 0;
}

