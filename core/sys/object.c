#include <syscall.h>
#include <object.h>
#include <processor.h>

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
		struct kso_invl_args ko;
		memcpy(&ko, &invl[i], sizeof(ko));
		if(ko.flags & KSOI_VALID) {
			struct object *o = obj_lookup(ko.id);
			if(o && __do_invalidate(o, &ko)) {
				suc++;
			}
			if(o) {
				obj_put(o);
			}
		}
	}
	return suc;
}

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags)
{
	objid_t paid = MKID(pahi, palo), chid = MKID(chhi, chlo);
	struct object *parent = paid == 0 ? kso_get_obj(current_thread->throbj, thr)
	                                  : obj_lookup(paid);
	struct object *child = obj_lookup(chid);

	if(!parent || !child) {
		if(child) obj_put(child);
		if(parent) obj_put(parent);
		return -1;
	}

	if(parent->kso_type == KSO_NONE || child->kso_type == KSO_NONE) {
		obj_put(child);
		obj_put(parent);
		return -1;
	}

	int ret = -1;
	if(child->kso_calls->attach) {
		ret = child->kso_calls->attach(parent, child, flags) ? 0 : -1;
	}

	obj_put(child);
	obj_put(parent);
	return ret;
}

long syscall_detach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags)
{
	objid_t paid = MKID(pahi, palo), chid = MKID(chhi, chlo);
	struct object *parent = paid == 0 ? kso_get_obj(current_thread->throbj, thr)
	                                  : obj_lookup(paid);
	struct object *child = obj_lookup(chid);

	if(!parent || !child) {
		if(child) obj_put(child);
		if(parent) obj_put(parent);
		return -1;
	}

	if(parent->kso_type == KSO_NONE || child->kso_type == KSO_NONE) {
		obj_put(child);
		obj_put(parent);
		return -1;
	}

	int ret = -1;
	if(child->kso_calls->detach) {
		ret = child->kso_calls->detach(parent, child, flags) ? 0 : -1;
	}

	obj_put(child);
	obj_put(parent);
	return ret;
}

long syscall_ocreate(uint64_t olo, uint64_t ohi, uint64_t tlo, uint64_t thi, uint64_t flags)
{
	objid_t id = MKID(ohi, olo);
	objid_t srcid = MKID(thi, tlo);
	int ksot = (flags >> 8) & 0xF;
	if(ksot >= KSO_MAX) {
		return -1;
	}
	if(id == 0) {
		return -1;
	}
	struct object *o;
	if(srcid) {
		o = obj_create_clone(id, srcid, ksot);
	} else {
		o = obj_create(id, ksot);
	}
	obj_put(o);
	return 0;
}

long syscall_odelete(uint64_t olo, uint64_t ohi, uint64_t flags)
{
	return 0;
}

