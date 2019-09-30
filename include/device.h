#pragma once

#include <interrupt.h>
#include <twz/driver/device.h>

struct object;
struct device {
	uint64_t uid;
	struct object *co;
	struct interrupt_alloc_req irs[MAX_DEVICE_INTERRUPTS];
	uint32_t flags;
};
void iommu_object_map_slot(struct device *dev, struct object *obj);
struct device_repr *device_get_repr(struct object *obj);
void *device_get_devspecific(struct object *obj);
void device_release_headers(struct object *obj);
struct object *device_register(uint32_t bustype, uint32_t devid);
void device_unregister(struct object *obj);
void device_signal_interrupt(struct object *obj, int inum, uint64_t val);
void device_signal_sync(struct object *obj, int snum, uint64_t val);
struct object *bus_register(uint32_t bustype, uint32_t busid, size_t bssz);
void *bus_get_busspecific(struct object *obj);
struct bus_repr *bus_get_repr(struct object *obj);
