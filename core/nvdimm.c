#include <device.h>
#include <init.h>
#include <kalloc.h>
#include <lib/rb.h>
#include <nvdimm.h>
#include <object.h>
#include <string.h>

#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/driver/memory.h>

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
	if(topinfo) {
		dev->topinfo = *topinfo;
		dev->flags |= NV_DEVICE_TOPINFO;
	}
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
	static _Atomic size_t __mono_id = 0;
	struct nv_region *reg = kalloc(sizeof(*reg));
	reg->start = start;
	reg->length = length;
	reg->ilinfo = ilinfo;
	reg->flags = flags;
	reg->id = id;
	reg->dev = dev;
	reg->mono_id = __mono_id++;
	rb_insert(&dev->regions_root, reg, struct nv_region, node, __nv_region_compar);
	printk("[nv] registered region %d (%lx -> %lx) for device %ld\n",
	  id,
	  start,
	  start + length - 1,
	  dev->id);
}

static struct object *nv_bus;

static void __init_nv_objects(void *_a __unused)
{
	nv_bus = bus_register(DEVICE_BT_NV, 0, 0);
	char name[128];
	snprintf(name, 128, "NVDIMM Bus");
	kso_setname(nv_bus, name);
	kso_root_attach(nv_bus, 0, KSO_DEVBUS);

	for(struct rbnode *dn = rb_first(&device_root); dn; dn = rb_next(dn)) {
		struct nv_device *dev = rb_entry(dn, struct nv_device, node);
		for(struct rbnode *rn = rb_first(&dev->regions_root); rn; rn = rb_next(rn)) {
			struct nv_region *reg = rb_entry(rn, struct nv_region, node);

			reg->obj = device_register(DEVICE_BT_NV, reg->mono_id);

			snprintf(name, 128, "NVDIMM Region %d:%d", reg->dev->id, reg->id);
			kso_setname(reg->obj, name);

			struct nv_header *hdr = device_get_devspecific(reg->obj);
			hdr->devid = reg->dev->id;
			hdr->regid = reg->id;

			kso_attach(nv_bus, reg->obj, reg->mono_id);
		}
	}
}
POST_INIT(__init_nv_objects, NULL);
