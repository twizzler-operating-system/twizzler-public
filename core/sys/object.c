#include <syscall.h>
#include <object.h>

static bool __do_invalidate(struct object *obj, struct kso_invl_args *invl)
{
	invl->result = KSO_INVL_RES_ERR;
	if(obj->kso_type != KSO_NONE && obj->kso_calls->invl) {
		return obj->kso_calls->invl(obj, invl);
	}
	return false;
}

long syscall_invalidate_kso(struct kso_invl_args *invl, size_t count)
{
	size_t suc = 0;
	for(size_t i=0;i<count;i++) {
		if(invl[i].flags & KSOI_VALID) {
			struct object *o = obj_lookup(invl[i].id);
			if(__do_invalidate(o, &invl[i])) {
				suc++;
			}
		}
	}
	return suc;
}

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags)
{
	objid_t paid = MKID(pahi, palo), chid = MKID(chhi, chlo);
	struct object *parent = obj_lookup(paid);
	struct object *child = obj_lookup(chid);

	if(!parent || !child) {
		return -1;
	}

	if(parent->kso_type == KSO_NONE || child->kso_type == KSO_NONE) {
		return -1;
	}

	int ret = -1;
	if(child->kso_calls->attach) {
		ret = child->kso_calls->attach(parent, child, flags) ? 0 : -1;
	}

	return ret;
}

long syscall_detach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags)
{
	objid_t paid = MKID(pahi, palo), chid = MKID(chhi, chlo);
	struct object *parent = obj_lookup(paid);
	struct object *child = obj_lookup(chid);

	if(!parent || !child) {
		return -1;
	}

	if(parent->kso_type == KSO_NONE || child->kso_type == KSO_NONE) {
		return -1;
	}

	int ret = -1;
	if(child->kso_calls->detach) {
		ret = child->kso_calls->detach(parent, child, flags) ? 0 : -1;
	}

	return ret;
}

long syscall_ocreate(uint64_t olo, uint64_t ohi, uint64_t tlo, uint64_t thi, uint64_t flags)
{
	return 0;
}

long syscall_odelete(uint64_t olo, uint64_t ohi, uint64_t flags)
{
	return 0;
}

