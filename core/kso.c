#include <device.h>
#include <object.h>
#include <spinlock.h>
#include <twz/_thrd.h>
#include <twz/driver/bus.h>
/* TODO: better system for tracking slots in the array */
static _Atomic size_t idx = 0;
static struct spinlock lock = SPINLOCK_INIT;

int kso_root_attach(struct object *obj, uint64_t flags, int type)
{
	(void)flags;
	struct object *root = obj_lookup(1, 0);
	spinlock_acquire_save(&lock);
	struct kso_attachment kar = {
		.flags = 0,
		.id = obj->id,
		.info = 0,
		.type = type,
	};
	size_t i = idx++;
	obj_write_data(root,
	  offsetof(struct kso_root_repr, attached) + sizeof(struct kso_attachment) * i,
	  sizeof(kar),
	  &kar);

	i++;
	obj_write_data(root, offsetof(struct kso_root_repr, count), sizeof(i), &i);

	spinlock_release_restore(&lock);
	obj_put(root);
	return i;
}

void kso_root_detach(int idx)
{
	struct object *root = obj_lookup(1, 0);
	spinlock_acquire_save(&lock);
	struct kso_attachment kar = {
		.flags = 0,
		.id = 0,
		.info = 0,
		.type = 0,
	};
	obj_write_data(root,
	  offsetof(struct kso_root_repr, attached) + sizeof(struct kso_attachment) * idx,
	  sizeof(kar),
	  &kar);

	spinlock_release_restore(&lock);
	obj_put(root);
}

/* TODO: detach threads when they exit */

void kso_attach(struct object *parent, struct object *child, size_t loc)
{
	assert(parent->kso_type);
	struct kso_attachment kar = {
		.type = child->kso_type,
		.id = child->id,
		.info = 0,
		.flags = 0,
	};
	switch(parent->kso_type) {
		size_t off;
		struct bus_repr *brepr;
		case KSO_DEVBUS:
			brepr = bus_get_repr(parent);
			off = (size_t)brepr->children - OBJ_NULLPAGE_SIZE;
			obj_write_data(parent, off + loc * sizeof(kar), sizeof(kar), &kar);
			if(brepr->max_children <= loc)
				brepr->max_children = loc + 1;

			break;
		default:
			panic("NI - kso_attach");
	}
}

#include <string.h>
void kso_setname(struct object *obj, const char *name)
{
	obj_write_data(obj, offsetof(struct kso_hdr, name), strlen(name) + 1, (void *)name);
}
