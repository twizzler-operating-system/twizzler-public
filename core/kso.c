#include <object.h>
#include <spinlock.h>
#include <twz/_thrd.h>
/* TODO: better system for tracking slots in the array */
static _Atomic size_t idx = 0;
static struct spinlock lock = SPINLOCK_INIT;

void kso_root_attach(struct object *obj, uint64_t flags, int type)
{
	(void)flags;
	struct object *root = obj_lookup(1);
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
}

/* TODO: detach threads when they exit */
