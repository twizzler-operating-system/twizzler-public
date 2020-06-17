#pragma once

struct nv_topology {
	uint8_t dimm_nr;
	uint8_t chan_nr;
	uint8_t ctrl_nr;
	uint8_t sock_nr;
	uint16_t node_nr;
	uint16_t flags;
};

struct nv_interleave {
	uint32_t nr_regions;
};

#define NV_DEVICE_TOPINFO 1

struct nv_device {
	struct rbnode node;
	uint64_t id;
	struct nv_topology topinfo;
	struct rbroot regions_root;
	uint32_t flags;
};

struct nv_region {
	uintptr_t start;
	uint64_t length;
	struct rbnode node;
	struct nv_interleave *ilinfo;
	struct object *obj;
	uint32_t id;
	uint32_t flags;
	uint64_t mono_id;
	struct nv_device *dev;
};

void nv_register_region(struct nv_device *dev,
  uint32_t id,
  uintptr_t start,
  uint64_t length,
  struct nv_interleave *ilinfo,
  uint32_t flags);

void nv_register_device(uint64_t id, struct nv_topology *topinfo);
struct nv_device *nv_lookup_device(uint64_t id);
struct nv_region *nv_lookup_region(struct nv_device *dev, uint32_t id);
