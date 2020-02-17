#include <device.h>
#include <kalloc.h>
#include <limits.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <slots.h>
#include <syscall.h>
#include <thread.h>
#include <twz/driver/bus.h>

static void __kso_device_ctor(struct object *obj)
{
	struct device *dev = obj->data = kalloc(sizeof(struct device));
	dev->co = obj;
	dev->flags = 0;
}

static struct kso_calls _kso_device = {
	.ctor = __kso_device_ctor,
};

__initializer static void __device_init(void)
{
	kso_register(KSO_DEVICE, &_kso_device);
}

struct bus_repr *bus_get_repr(struct object *obj)
{
	/* repr is at object base */
	return obj_get_kbase(obj);
}

void *bus_get_busspecific(struct object *obj)
{
	struct bus_repr *repr = bus_get_repr(obj);
	return (void *)(repr + 1);
}

struct device_repr *device_get_repr(struct object *obj)
{
	return obj_get_kbase(obj);
}

void *device_get_devspecific(struct object *obj)
{
	struct device_repr *repr = device_get_repr(obj);
	return (void *)(repr + 1);
}

void device_release_headers(struct object *obj)
{
	obj_release_kaddr(obj);
	/* TODO: release the address */
	// struct objpage *op = obj_get_page(obj, OBJ_NULLPAGE_SIZE, true);
	// obj_put_page(op); /* once for this function */
	// obj_put_page(op); /* once for device_get_<header> */
}

void device_signal_interrupt(struct object *obj, int inum, uint64_t val)
{
	/* TODO: try to make this more efficient */
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
	struct device *data = obj->data;
	assert(data != NULL);

	for(size_t i = 0; i < count; i++) {
		data->irs[i] = (struct interrupt_alloc_req){
			.flags = INTERRUPT_ALLOC_REQ_VALID,
			.handler.fn = __device_interrupt,
			.handler.devobj = obj,
			.handler.arg = i,
		};
	}
	if(interrupt_allocate_vectors(count, data->irs)) {
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

static long __device_kaction(struct object *obj, long op, long arg)
{
	int ret = -ENOTSUP;
	switch(op) {
		case KACTION_CMD_DEVICE_SETUP_INTERRUPTS:
			ret = __device_alloc_interrupts(obj, arg);
			break;
	}
	return ret;
}

struct object *device_register(uint32_t bustype, uint32_t devid)
{
	int r;
	objid_t psid;
	/* TODO: restrict write access. In fact, do this for ALL KSOs. */
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &psid);
	if(r < 0)
		panic("failed to create device object: %d", r);
	struct object *obj = obj_lookup(psid, OBJ_LOOKUP_HIDDEN);
	assert(obj != NULL);
	obj->kaction = __device_kaction;
	obj_kso_init(obj, KSO_DEVICE);
	struct device *data = obj->data;
	data->uid = ((uint64_t)bustype << 32) | devid;

	struct device_repr *repr = device_get_repr(obj);
	repr->device_bustype = bustype;
	repr->device_id = devid;
	device_release_headers(obj);
	return obj; /* krc: move */
}

struct object *bus_register(uint32_t bustype, uint32_t busid, size_t bssz)
{
	int r;
	objid_t psid;
	/* TODO: restrict write access. In fact, do this for ALL KSOs. */
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &psid);
	if(r < 0)
		panic("failed to create bus object: %d", r);
	struct object *obj = obj_lookup(psid, OBJ_LOOKUP_HIDDEN);
	assert(obj != NULL);
	// obj->kaction = __device_kaction;
	obj_kso_init(obj, KSO_DEVBUS);

	struct bus_repr *repr = bus_get_repr(obj);
	repr->bus_id = busid;
	repr->bus_type = bustype;
	repr->children = (void *)(sizeof(struct bus_repr) + bssz + OBJ_NULLPAGE_SIZE);
	device_release_headers(obj);
	return obj; /* krc: move */
}

void device_unregister(struct object *obj)
{
	panic("NI - device_unregister %p", obj);
}
