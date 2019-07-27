#include <object.h>
#include <twz/_thrd.h>

/* TODO: better system for tracking slots in the array */
static _Atomic size_t idx = 0;

void kso_root_attach(struct object *obj, uint64_t flags, int type)
{
	struct object *root = obj_lookup(1);
	struct kso_attachment kar = {
		.flags = 0,
		.id = obj->id,
		.info = 0,
		.type = KSO_THREAD,
	};
	size_t i = idx++;
	obj_write_data(root,
	  offsetof(struct kso_root_repr, attached) + sizeof(struct kso_attachment) * i,
	  sizeof(kar),
	  &kar);

	i++;
	obj_write_data(root, offsetof(struct kso_root_repr, count), sizeof(i), &i);

	obj_put(root);
}

/* TODO: detach threads when they exit */
