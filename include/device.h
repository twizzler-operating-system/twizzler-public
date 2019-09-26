#pragma once

struct object;
struct device {
	uint16_t flags;
	uint16_t type;
	uint32_t did;

	struct object *co;
};
void iommu_object_map_slot(struct device *dev, struct object *obj);
