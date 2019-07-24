#include <object.h>
#include <processor.h>
#include <rand.h>
#include <syscall.h>

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
	for(size_t i = 0; i < count; i++) {
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

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t ft)
{
	objid_t paid = MKID(pahi, palo), chid = MKID(chhi, chlo);
	uint32_t type = (ft & 0xffffffff);
	uint32_t flags = (ft >> 32) & 0xffffffff;
	struct object *parent = paid == 0 ? kso_get_obj(current_thread->throbj, thr) : obj_lookup(paid);
	struct object *child = obj_lookup(chid);

	if(!parent || !child) {
		if(child)
			obj_put(child);
		if(parent)
			obj_put(parent);
		return -1;
	}

	spinlock_acquire_save(&child->lock);
	/* don't need lock on parent since kso_type is atomic, and once set cannot be unset */
	if(parent->kso_type == KSO_NONE || (child->kso_type != KSO_NONE && child->kso_type != type)) {
		spinlock_release_restore(&child->lock);
		obj_put(child);
		obj_put(parent);
		return -1;
	}
	if(child->kso_type == KSO_NONE) {
		obj_kso_init(child, type);
	}
	spinlock_release_restore(&child->lock);

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
	struct object *parent = paid == 0 ? kso_get_obj(current_thread->throbj, thr) : obj_lookup(paid);
	struct object *child = obj_lookup(chid);

	if(!parent || !child) {
		if(child)
			obj_put(child);
		if(parent)
			obj_put(parent);
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

long syscall_ocreate(uint64_t kulo,
  uint64_t kuhi,
  uint64_t slo,
  uint64_t shi,
  uint64_t flags,
  objid_t *retid)
{
	objid_t kuid = MKID(kuhi, kulo);
	objid_t srcid = MKID(shi, slo);
	nonce_t nonce = 0;
	int r;
	if(!(flags & TWZ_SYS_OC_ZERONONCE)) {
		r = rand_getbytes(&nonce, sizeof(nonce), 0);
		if(r < 0) {
			return r;
		}
	}
	int ksot = (flags >> 8) & 0xF;
	if(ksot >= KSO_MAX) {
		return -1;
	}
	struct object *o;
	if(srcid) {
		o = obj_create_clone(0, srcid, ksot);
	} else {
		o = obj_create(0, ksot);
	}

	struct metainfo mi = {
		.p_flags =
		  flags & (MIP_HASHDATA | MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE),
		.flags = flags,
		.milen = sizeof(mi) + 128,
		.mdbottom = 0x1000, // TODO: not if we're copying?
		.kuid = kuid,
		.nonce = nonce,
	};

	obj_write_data(o, OBJ_MAXSIZE - (OBJ_NULLPAGE_SIZE + OBJ_METAPAGE_SIZE), sizeof(mi), &mi);

	objid_t id = obj_compute_id(o);
	obj_assign_id(o, id);
	obj_put(o);

	if(retid)
		*retid = id;
	return 0;
}

long syscall_odelete(uint64_t olo, uint64_t ohi, uint64_t flags)
{
	return 0;
}
