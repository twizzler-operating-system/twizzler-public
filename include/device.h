#pragma once

#include <twz/driver/device.h>

struct object;
struct device {
	uint64_t uid;
	struct object *co;
	struct interrupt_alloc_req irs[MAX_DEVICE_INTERRUPTS];
	uint32_t flags;
};
void iommu_object_map_slot(struct device *dev, struct object *obj);
