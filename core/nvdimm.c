#include <device.h>
#include <init.h>
#include <kalloc.h>
#include <lib/rb.h>
#include <memory.h>
#include <nvdimm.h>
#include <object.h>
#include <page.h>
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

/* a region is broken down into page groups. The page groups start with 2 meta data pages, followed
 * by data pages. The first metadata page is unused unless it's the first page group, in which case
 * it contains the superpage. The second metadata page is a bitmap indicating which blocks are
 * allocated. This bitmap includes the two metadata pages, thus they are always marked as allocated.
 */

#define NVD_HDR_MAGIC 0x12345678

struct nvdimm_region_header {
	uint32_t magic;
	uint32_t version;
	uint64_t flags;

	uint64_t metaobj_root;
	uint64_t pad;

	uint32_t pg_used_num[];
};

#define PGGRP_LEN ({ (mm_page_size(0) * 8) * mm_page_size(0); })

#define PGGRP_ALLOC_PAGE(reg, i)                                                                   \
	({                                                                                             \
		(uint8_t *)((char *)obj_get_kbase(reg->metaobj) + mm_page_size(0) * i * 2                  \
		            + mm_page_size(0));                                                            \
	})

static uint64_t nv_region_alloc_page(struct nv_region *reg)
{
	struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);
	spinlock_acquire_save(&reg->lock);

	size_t nr = reg->length / PGGRP_LEN;

	for(size_t i = 0; i < nr; i++) {
		if(hdr->pg_used_num[i] < PGGRP_LEN / mm_page_size(0)) {
			hdr->pg_used_num[i]++;

			uint8_t *bm = PGGRP_ALLOC_PAGE(reg, i);
			for(size_t j = 0; j < PGGRP_LEN / mm_page_size(0); j++) {
				if(!(bm[j / 8] & (1 << (j % 8)))) {
					bm[j / 8] |= (1 << (j % 8));
					spinlock_release_restore(&reg->lock);
					return i * PGGRP_LEN + j * mm_page_size(0);
				}
			}
		}
	}
	spinlock_release_restore(&reg->lock);
	return 0;
}

static void nv_init_region_contents(struct nv_region *reg)
{
	struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);

	hdr->version = 0;
	hdr->flags = 0;
	size_t nr = reg->length / PGGRP_LEN;
	printk("[nv]   init %ld pagegroups\n", nr);
	for(size_t i = 0; i < nr; i++) {
		uint8_t *bm = PGGRP_ALLOC_PAGE(reg, i);
		memset(bm, 0, mm_page_size(0));
		bm[0] |= 3;
		hdr->pg_used_num[i] = 2;
	}

	uint64_t p = nv_region_alloc_page(reg);
	printk("Alloced page %lx\n", p);
}

static void nv_init_region(struct nv_region *reg)
{
	/* TODO: actual ID allocation for system objects */
	reg->metaobj = obj_create(reg->mono_id | 0x800000000, 0);
	reg->metaobj->flags |= OF_KERNEL;
	reg->lock = SPINLOCK_INIT;

	size_t nr = reg->length / PGGRP_LEN;
	for(size_t i = 0; i < nr; i++) {
		struct page *pg = page_alloc_nophys();
		pg->addr = reg->start + PGGRP_LEN * i;
		pg->level = 0;
		pg->type = PAGE_TYPE_PERSIST;
		pg->flags |= PAGE_CACHE_WB;
		obj_cache_page(reg->metaobj, OBJ_NULLPAGE_SIZE + mm_page_size(0) * i * 2, pg);

		pg = page_alloc_nophys();
		pg->addr = reg->start + PGGRP_LEN * i + mm_page_size(0);
		pg->level = 0;
		pg->type = PAGE_TYPE_PERSIST;
		pg->flags |= PAGE_CACHE_WB;
		obj_cache_page(
		  reg->metaobj, OBJ_NULLPAGE_SIZE + mm_page_size(0) * i * 2 + mm_page_size(0), pg);
	}

	struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);
	if(hdr->magic != NVD_HDR_MAGIC) {
		printk("[nv] initializing contents of region %ld\n", reg->mono_id);
		nv_init_region_contents(reg);
	}
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
			nv_init_region(reg);
		}
	}
}
POST_INIT(__init_nv_objects, NULL);
