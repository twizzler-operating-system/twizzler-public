#include <object.h>
#include <processor.h>
#include <rand.h>
#include <syscall.h>
#include <twz/_sctx.h>

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

long syscall_kaction(size_t count, struct sys_kaction_args *args)
{
	size_t t = 0;
	if(count > 4096 || !args)
		return -EINVAL;
	for(size_t i = 0; i < count; i++) {
		if(args[i].flags & KACTION_VALID) {
			struct object *obj = obj_lookup(args->id);
			if(!obj) {
				args[i].result = -ENOENT;
				continue;
			}
			if(obj->kaction) {
				args[i].result = obj->kaction(obj, args[i].cmd, args[i].arg);
				t++;
			}
			obj_put(obj);
		}
	}
	return t;
}

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t ft)
{
	objid_t paid = MKID(pahi, palo), chid = MKID(chhi, chlo);
	uint16_t type = (ft & 0xffff);
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

	int e;
	if(paid && (e = obj_check_permission(parent, SCP_USE | SCP_WRITE)) != 0) {
		return e;
	}

	if((e = obj_check_permission(child, SCP_USE)) != 0) {
		return e;
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
	if(child->kso_calls && child->kso_calls->attach) {
		ret = child->kso_calls->attach(parent, child, flags) ? 0 : -1;
	}

	obj_put(child);
	obj_put(parent);
	return ret;
}

long syscall_detach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t ft)
{
	uint16_t type = ft & 0xffff;
	uint16_t sysc = (ft >> 16) & 0xffff;
	uint32_t flags = (ft >> 32) & 0xffffffff;

	if(type >= KSO_MAX)
		return -EINVAL;

	objid_t paid = MKID(pahi, palo), chid = MKID(chhi, chlo);
	struct object *parent = paid == 0 ? kso_get_obj(current_thread->throbj, thr) : obj_lookup(paid);
	struct object *child = chid == 0 ? NULL : obj_lookup(chid);

	if(!parent) {
		if(child)
			obj_put(child);
		return -1;
	}

	int e;
	if(paid && (e = obj_check_permission(parent, SCP_USE | SCP_WRITE)) != 0) {
		return e;
	}

	if(child && (e = obj_check_permission(child, SCP_USE)) != 0) {
		return e;
	}

	if(parent->kso_type == KSO_NONE || (child && child->kso_type == KSO_NONE)
	   || (child && child->kso_type != type && type != KSO_NONE)) {
		if(child)
			obj_put(child);
		obj_put(parent);
		printk("B\n");
		return -1;
	}

	int ret = -1;
	if(sysc == SYS_NULL)
		sysc = SYS_DETACH;
	if(child && child->kso_calls && child->kso_calls->detach) {
		ret = child->kso_calls->detach(parent, child, sysc, flags) ? 0 : -1;
	} else if(!child) {
		struct kso_calls *kc = kso_lookup_calls(type);
		bool (*c)(struct object *, struct object *, int, int) = kc ? kc->detach : NULL;
		if(c) {
			ret = c(parent, child, sysc, flags) ? 0 : -1;
		}
	}

	if(child)
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
		struct object *so = obj_lookup(srcid);
		if(!so) {
			return -ENOENT;
		}
		if((r = obj_check_permission(so, SCP_READ))) {
			obj_put(so);
			return r;
		}
		o = obj_create_clone(0, srcid, ksot);
		obj_put(so);
	} else {
		o = obj_create(0, ksot);
	}

	struct metainfo mi = {
		.magic = MI_MAGIC,
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

long syscall_opin(uint64_t lo, uint64_t hi, uint64_t *addr, int flags)
{
	objid_t id = MKID(hi, lo);
	struct object *o = obj_lookup(id);
	if(!o)
		return -ENOENT;

	if(flags & OP_UNPIN) {
		o->pinned = false;
	} else {
		o->pinned = true;
		obj_alloc_slot(o);
		if(addr)
			*addr = o->slot * OBJ_MAXSIZE;
	}
	obj_put(o);
	return 0;
}

#include <device.h>
#include <page.h>
long syscall_octl(uint64_t lo, uint64_t hi, int op, long arg1, long arg2, long arg3)
{
	objid_t id = MKID(hi, lo);
	struct object *o = obj_lookup(id);
	if(!o)
		return -ENOENT;

	int r = 0;
	switch(op) {
		size_t pnb, pne;
		case OCO_CACHE_MODE:
			o->cache_mode = arg3;
			pnb = arg1 / mm_page_size(0);
			pne = (arg1 + arg2) / mm_page_size(0);
			/* TODO: bounds check */
			for(size_t i = pnb; i < pne; i++) {
				struct objpage *pg = obj_get_page(o, i, true);
				/* TODO: locking */
				pg->page->flags |= flag_if_notzero(arg3 & OC_CM_UC, PAGE_CACHE_UC);
				pg->page->flags |= flag_if_notzero(arg3 & OC_CM_WB, PAGE_CACHE_WB);
				pg->page->flags |= flag_if_notzero(arg3 & OC_CM_WT, PAGE_CACHE_WT);
				pg->page->flags |= flag_if_notzero(arg3 & OC_CM_WC, PAGE_CACHE_WC);
				arch_object_map_page(o, pg->page, i);
			}
			break;
		case OCO_MAP:
			pnb = arg1 / mm_page_size(0);
			pne = (arg1 + arg2) / mm_page_size(0);

			/* TODO: bounds check */
			for(size_t i = pnb; i < pne; i++) {
				struct objpage *pg = obj_get_page(o, i, true);
				/* TODO: locking */
				arch_object_map_page(o, pg->page, i);
				if(arg3)
					arch_object_map_flush(o, i);
			}
			if(arg3) {
				objid_t doid = *(objid_t *)arg3;
				struct object *dobj = obj_lookup(doid);
				if(!dobj) {
					return -ENOENT;
				}
				if(dobj->kso_type != KSO_DEVICE)
					return -EINVAL;
				/* TODO: actually lookup the device */
				iommu_object_map_slot(dobj->data, o);
			}

			break;
		default:
			r = -EINVAL;
	}

	obj_put(o);
	return r;
}
