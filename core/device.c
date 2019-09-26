#include <object.h>

struct device_repr *device_get_repr(struct object *obj)
{
}

void *device_get_devspecific(struct object *obj)
{
}

void device_release_headers(struct object *obj)
{
}

struct object *device_register(uint32_t bustype, uint32_t devid)
{
}

void device_unregister(struct object *obj)
{
}

void device_signal_interrupt(struct object *obj, int inum, uint64_t val)
{
	obj_write_data_atomic64(obj, offsetof(struct device_repr, interrupts[inum]), val);
	thread_wake_object(
	  obj, offsetof(struct device_repr, interrupts[inum]) + OBJ_NULLPAGE_SIZE, INT_MAX);
}

void device_signal_sync(struct object *obj, int snum, uint64_t val)
{
	obj_write_data_atomic64(obj, offsetof(struct device_repr, syncs[snum]), val);
	thread_wake_object(obj, offsetof(struct device_repr, syncs[snum]) + OBJ_NULLPAGE_SIZE, INT_MAX);
}

static void __device_interrupt(int v, struct interrupt_handler *ih)
{
	device_signal_interrupt(ih->devobj, ih->arg, v);
}

static int __device_alloc_interrupts(struct object *obj, size_t count)
{
	if(count > MAX_DEVICE_INTERRUPTS)
		return -EINVAL;

	int ret;
	struct device_repr *repr = device_get_repr(obj);
	struct device_data *data = obj->data;
	assert(data);

	for(size_t i = 0; i < count; i++) {
		data->irs = (struct interrupt_alloc_req){
			.flags = INTERRUPT_ALLOC_REQ_VALID,
			.handler.fn = __device_interrupt,
			.handler.devobj = obj,
			.handler.arg = i,
		};
	}
	if(interrupt_allocate_vectors(hdr.nr_interrupts, data->ir)) {
		ret = -EIO;
		goto out;
	}

	for(size_t i = 0; i < count; i++) {
		if(data->irs[i].flags & INTERRUPT_ALLOC_REQ_ENABLED) {
			repr->interrupts[i].vec = data->irs[i].vec;
		}
	}

	ret = 0;
out:
	device_release_headers(obj);
	return ret;
}

static int __device_kaction(struct object *obj, int op, long arg)
{
	int ret = -ENOTSUP;
	switch(op) {
		case KACTION_CMD_DEVICE_SETUP_INTERRUPTS:
			ret = __device_alloc_interrupts(obj, arg);
			break;
	}
	return ret;
}
