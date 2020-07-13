#include <device.h>
#include <init.h>
#include <kalloc.h>
#include <lib/iter.h>
#include <lib/rb.h>
#include <memory.h>
#include <nvdimm.h>
#include <object.h>
#include <page.h>
#include <processor.h>
#include <string.h>
#include <tmpmap.h>

#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/driver/memory.h>

RB_DECLARE_STANDARD_COMPARISONS(nv_device, uint64_t, id);

RB_DECLARE_STANDARD_COMPARISONS(nv_region, uint32_t, id);

static struct rbroot device_root = RBINIT;

static DECLARE_LIST(reg_list);

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

/* a region is broken down into page groups. The page groups start with meta data pages, followed
 * by data pages. The first metadata page is unused unless it's the first page group, in which case
 * it contains the superpage. The second metadata page is a bitmap indicating which blocks are
 * allocated. The rest of the metadata pages contain hash buckets for lookups of (id, pgnr).
 * The allocation bitmap includes the metadata pages, thus they are always marked as allocated.
 * TODO: take this into account in page allocation.
 */

struct nvdimm_bucket {
	objid_t id;
	uint64_t reg_page;
	uint32_t obj_page;
	uint32_t flags;
};

struct nvdimm_pggrp {
	uint8_t sb[4096];
	uint8_t bm[4096];
	struct nvdimm_bucket buckets[];
};

#define PGGRP_LEN ({ (mm_page_size(0) * 8) * mm_page_size(0); })

#define BUCKETS_PER_GROUP ({ (PGGRP_LEN / mm_page_size(0)) * 2; })

#define META_GROUP_LEN ({ mm_page_size(0) * 2 + BUCKETS_PER_GROUP * sizeof(struct nvdimm_bucket); })

#define PGGRP_META(reg, i)                                                                         \
	({ (struct nvdimm_pggrp *)((char *)obj_get_kbase(reg->metaobj) + i * META_GROUP_LEN); })

static uint64_t __nv_region_alloc_page(struct nv_region *reg)
{
	struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);

	size_t nr = reg->length / PGGRP_LEN;

	for(size_t i = 0; i < nr; i++) {
		if(hdr->pg_used_num[i] < PGGRP_LEN / mm_page_size(0)) {
			hdr->pg_used_num[i]++;
			hdr->used_pages++;
			arch_processor_clwb(hdr->pg_used_num[i]);
			arch_processor_clwb(hdr->used_pages);

			struct nvdimm_pggrp *pgg = PGGRP_META(reg, i);
			for(size_t j = 0; j < PGGRP_LEN / mm_page_size(0); j++) {
				if(!(pgg->bm[j / 8] & (1 << (j % 8)))) {
					pgg->bm[j / 8] |= (1 << (j % 8));
					arch_processor_clwb(pgg->bm[j / 8]);
					return i * PGGRP_LEN + j * mm_page_size(0);
				}
			}
		}
	}
	return 0;
}

static void __nv_region_free_page(struct nv_region *reg, uint64_t pg)
{
	size_t grp = pg / PGGRP_LEN;
	size_t wg = (pg / mm_page_size(0)) % PGGRP_LEN;

	struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);
	struct nvdimm_pggrp *pgg = PGGRP_META(reg, grp);
	pgg->bm[wg / 8] &= ~(1 << (wg % 8));
	arch_processor_clwb(pgg->bm[wg / 8]);
	hdr->used_pages--;
	hdr->pg_used_num[grp]--;
	arch_processor_clwb(hdr->used_pages);
	arch_processor_clwb(hdr->pg_used_num[wg]);
}

static uint64_t __hash(objid_t id, uint32_t pgnr, uint64_t mod)
{
	objid_t _p = pgnr;
	objid_t tmp = ((id ^ _p) ^ (id >> (uint64_t)(pgnr % 64u)));
	return (uint64_t)tmp % mod;
}

static int __nv_region_delete_group(struct nv_region *reg, size_t group, objid_t id, uint32_t pgnr)
{
	struct nvdimm_pggrp *pgg = PGGRP_META(reg, group);

	size_t b = __hash(id, pgnr, BUCKETS_PER_GROUP);
	size_t i = b;
	do {
		struct nvdimm_bucket *bucket = &pgg->buckets[i];
		if(bucket->id == id && bucket->obj_page) {
			bucket->flags = 1;
			arch_processor_clwb(bucket->flags);
			atomic_thread_fence(memory_order_acq_rel);
			bucket->id = 0;
			arch_processor_clwb(bucket->id);
			return 2;
		}
		if(bucket->flags == 0) {
			return 0;
		}
		i = (i + 1) % BUCKETS_PER_GROUP;
	} while(i != b);
	return 1;
}

static int __nv_region_delete(struct nv_region *reg, objid_t id, uint32_t pgnr)
{
	size_t gnr = reg->length / PGGRP_LEN;
	size_t b = __hash(id, pgnr, gnr);

	size_t i = b;
	do {
		uint64_t ret = __nv_region_delete_group(reg, i, id, pgnr);
		if(ret == 0) {
			return 0;
		}
		if(ret != 1) {
			return ret;
		}
		i = (i + 1) % gnr;
	} while(i != b);
	return 0;
}

static uint64_t __nv_region_lookup_group(struct nv_region *reg,
  size_t group,
  objid_t id,
  uint32_t pgnr)
{
	struct nvdimm_pggrp *pgg = PGGRP_META(reg, group);

	size_t b = __hash(id, pgnr, BUCKETS_PER_GROUP);
	size_t i = b;
	do {
		struct nvdimm_bucket *bucket = &pgg->buckets[i];
		if(bucket->id == id && bucket->obj_page) {
			return bucket->reg_page;
		}
		if(bucket->flags == 0) {
			return 0;
		}
		i = (i + 1) % BUCKETS_PER_GROUP;
	} while(i != b);
	return 1;
}

static uint64_t __nv_region_lookup(struct nv_region *reg, objid_t id, uint32_t pgnr)
{
	size_t gnr = reg->length / PGGRP_LEN;
	size_t b = __hash(id, pgnr, gnr);

	size_t i = b;
	do {
		uint64_t ret = __nv_region_lookup_group(reg, i, id, pgnr);
		if(ret == 0) {
			return 0;
		}
		if(ret != 1) {
			return ret;
		}
		i = (i + 1) % gnr;
	} while(i != b);
	return 0;
}

static int __nv_region_insert_group(struct nv_region *reg,
  size_t group,
  objid_t id,
  uint32_t pgnr,
  uint64_t regpage)
{
	struct nvdimm_pggrp *pgg = PGGRP_META(reg, group);

	size_t b = __hash(id, pgnr, BUCKETS_PER_GROUP);
	size_t i = b;
	do {
		struct nvdimm_bucket *bucket = &pgg->buckets[i];
		if(bucket->id == 0) {
			bucket->obj_page = pgnr;
			bucket->reg_page = regpage;
			arch_processor_clwb(bucket->obj_page);
			arch_processor_clwb(bucket->reg_page);
			bucket->id = id;
			arch_processor_clwb(bucket->id);
			return 0;
		}
		i = (i + 1) % BUCKETS_PER_GROUP;
	} while(i != b);
	return -ENOSPC;
}

static int __nv_region_insert(struct nv_region *reg, objid_t id, uint32_t pgnr, uint64_t regpage)
{
	size_t gnr = reg->length / PGGRP_LEN;
	size_t b = __hash(id, pgnr, gnr);

	size_t i = b;
	do {
		if(__nv_region_insert_group(reg, i, id, pgnr, regpage) == 0) {
			return 0;
		}
		i = (i + 1) % gnr;
	} while(i != b);
	return -ENOSPC;
}

static void __zero(struct nv_region *reg, uint64_t p)
{
	struct page page;
	page.addr = p + reg->start;
	page.level = 0;
	void *s = tmpmap_map_page(&page);
	memset(s, 0, mm_page_size(0));
	tmpmap_unmap_page(s);
}

static uint64_t nv_region_lookup_or_create(struct nv_region *reg, objid_t id, uint32_t pgnr)
{
	spinlock_acquire_save(&reg->lock);
	if(id == 0) {
		uint64_t p = __nv_region_alloc_page(reg);
		spinlock_release_restore(&reg->lock);
		__zero(reg, p);
		return p;
	}
	uint64_t p = __nv_region_lookup(reg, id, pgnr);
	if(p == 0) {
		p = __nv_region_alloc_page(reg);
		__zero(reg, p); // TODO: try to move this out of the lock
		if(__nv_region_insert(reg, id, pgnr, p)) {
			panic("TODO: out of space in region");
		}
	}
	spinlock_release_restore(&reg->lock);
	return p;
}

int nv_region_persist_obj_meta(struct object *obj)
{
	struct objpage *p;
	enum obj_get_page_result gpr = obj_get_page(obj, OBJ_MAXSIZE - mm_page_size(0), &p, 0);
	if(gpr != GETPAGE_OK) {
		panic(
		  "failed to get meta page %ld when persisting object", OBJ_MAXSIZE / mm_page_size(0) - 1);
	}

	spinlock_acquire_save(&obj->preg->lock);

	if(__nv_region_insert(obj->preg, obj->id, p->idx, p->page->addr - obj->preg->start)) {
		panic("TODO: out of space in region");
	}
	spinlock_release_restore(&obj->preg->lock);
	return 0;
}

void nv_region_free_page(struct object *obj, uint32_t pgnr, uint64_t addr)
{
	spinlock_acquire_save(&obj->preg->lock);
	__nv_region_delete(obj->preg, obj->id, pgnr);
	__nv_region_free_page(obj->preg, addr - obj->preg->start);
	spinlock_release_restore(&obj->preg->lock);
}

struct page *nv_region_pagein(struct object *obj, size_t idx)
{
	// printk("[nv] pagein " IDFMT " :: %ld\n", IDPR(obj->id), idx);
	struct page *pg = page_alloc_nophys();
	pg->addr = obj->preg->start + nv_region_lookup_or_create(obj->preg, obj->id, idx);
	pg->level = 0;
	pg->type = PAGE_TYPE_PERSIST;
	pg->flags |= PAGE_CACHE_WB;
	return pg;
}

static void nv_init_region_contents(struct nv_region *reg)
{
	struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);

	size_t nr = reg->length / PGGRP_LEN;
	for(size_t i = 0; i < nr; i++) {
		struct nvdimm_pggrp *pgg = PGGRP_META(reg, i);
		memset(pgg, 0, META_GROUP_LEN);
		for(size_t p = 0; p < META_GROUP_LEN / mm_page_size(0); p++) {
			pgg->bm[p / 8] |= (1 << (p % 8));
			arch_processor_clwb(pgg->bm[p / 8]);
		}
		hdr->pg_used_num[i] = META_GROUP_LEN / mm_page_size(0);
		arch_processor_clwb(hdr->pg_used_num[i]);
	}

	hdr->version = 0;
	hdr->flags = 0;
	hdr->total_pages = (reg->length - META_GROUP_LEN * nr) / mm_page_size(0);
	hdr->used_pages = 0;
	arch_processor_clwb(hdr->version);
	arch_processor_clwb(hdr->flags);
	arch_processor_clwb(hdr->total_pages);
	arch_processor_clwb(hdr->used_pages);
	printk("[nv]   init %ld pagegroups\n", nr);
	printk("[nv]   mgl = %ld-ish MB\n", META_GROUP_LEN / (1024 * 1024));
	printk("[nv]   total pages: %ld (%ld MB)\n",
	  hdr->total_pages,
	  (mm_page_size(0) * hdr->total_pages) / (1024 * 1024));
	hdr->magic = NVD_HDR_MAGIC;
	arch_processor_clwb(hdr->magic);
}

static void nv_init_region(struct nv_region *reg)
{
	/* TODO: actual ID allocation for system objects */
	reg->metaobj = obj_create(reg->mono_id | 0x800000000, 0);
	reg->metaobj->flags |= OF_KERNEL;
	reg->lock = SPINLOCK_INIT;

	size_t nr = reg->length / PGGRP_LEN;
	if(nr * META_GROUP_LEN + OBJ_NULLPAGE_SIZE >= OBJ_MAXSIZE) {
		panic("TODO: large NVDIMM region");
	}
	for(size_t i = 0; i < nr; i++) {
		for(size_t p = 0; p < META_GROUP_LEN / mm_page_size(0); p++) {
			struct page *pg = page_alloc_nophys();
			pg->addr = reg->start + PGGRP_LEN * i + p * mm_page_size(0);
			pg->level = 0;
			pg->type = PAGE_TYPE_PERSIST;
			pg->flags |= PAGE_CACHE_WB;
			obj_cache_page(
			  reg->metaobj, OBJ_NULLPAGE_SIZE + META_GROUP_LEN * i + p * mm_page_size(0), pg);
		}
	}

	struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);
	if(hdr->magic != NVD_HDR_MAGIC || 0) {
		printk("[nv] initializing contents of region %ld\n", reg->mono_id);
		nv_init_region_contents(reg);
	}

	list_insert(&reg_list, &reg->entry);
}

struct nv_region *nv_region_lookup_object(objid_t id)
{
	foreach(e, list, &reg_list) {
		struct nv_region *reg = list_entry(e, struct nv_region, entry);
		spinlock_acquire_save(&reg->lock);
		bool found =
		  !!__nv_region_lookup(reg, id, (OBJ_MAXSIZE - mm_page_size(0)) / mm_page_size(0));
		if(found) {
			spinlock_release_restore(&reg->lock);
			return reg;
		}
		spinlock_release_restore(&reg->lock);
	}

	return NULL;
}

struct nv_region *nv_region_select(void)
{
	foreach(e, list, &reg_list) {
		struct nv_region *reg = list_entry(e, struct nv_region, entry);
		struct nvdimm_region_header *hdr = obj_get_kbase(reg->metaobj);
		if(hdr->total_pages > (hdr->used_pages + 4096)) {
			/* try this region */
			return reg;
		}
	}

	return NULL;
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
			hdr->meta_lo = ID_LO(reg->metaobj->id);
			hdr->meta_hi = ID_HI(reg->metaobj->id);
		}
	}
}
POST_INIT(__init_nv_objects, NULL);
