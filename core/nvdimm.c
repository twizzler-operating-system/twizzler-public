#include <kalloc.h>
#include <lib/rb.h>
#include <nvdimm.h>

RB_DECLARE_STANDARD_COMPARISONS(nv_device, uint64_t, id);

RB_DECLARE_STANDARD_COMPARISONS(nv_region, uint32_t, id);

static struct rbroot device_root = RBINIT;

struct nv_region *nv_lookup_region(struct nv_device *dev, uint32_t id)
{
	struct rbnode *node =
	  rb_search(&dev->regions_root, id, struct nv_region, node, __nv_region_compar_key);
	return node ? rb_entry(node, struct nv_region, node) : NULL;
}

struct nv_device *nv_lookup_device(uint64_t id)
{
	struct rbnode *node =
	  rb_search(&device_root, id, struct nv_device, node, __nv_device_compar_key);
	return node ? rb_entry(node, struct nv_device, node) : NULL;
}

void nv_register_device(uint64_t id, struct nv_topology *topinfo)
{
	struct nv_device *dev = kalloc(sizeof(*dev));
	dev->id = id;
	dev->topinfo = topinfo;
	dev->regions_root = RBINIT;
	rb_insert(&device_root, dev, struct nv_device, node, __nv_device_compar);
	printk("[nv] registered device %ld\n", id);
}

void nv_register_region(struct nv_device *dev,
  uint32_t id,
  uintptr_t start,
  uint64_t length,
  struct nv_interleave *ilinfo,
  uint32_t flags)
{
	struct nv_region *reg = kalloc(sizeof(*reg));
	reg->start = start;
	reg->length = length;
	reg->ilinfo = ilinfo;
	reg->flags = flags;
	reg->id = id;
	rb_insert(&dev->regions_root, reg, struct nv_region, node, __nv_region_compar);
	printk("[nv] registered region %d (%lx -> %lx) for device %ld\n",
	  id,
	  start,
	  start + length - 1,
	  dev->id);
}
