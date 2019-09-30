#pragma once
#include <twz/_kso.h>

struct bus_repr {
	struct kso_hdr hdr;
	uint32_t bus_type;
	uint32_t bus_id;
	size_t max_children;
	struct kso_attachment *children;
};

#ifndef __KERNEL__

#include <errno.h>
#include <twz/obj.h>

static inline struct bus_repr *twz_bus_getrepr(struct object *obj)
{
	return twz_obj_base(obj);
}

static inline void *twz_bus_getbs(struct object *obj)
{
	return (void *)(twz_bus_getrepr(obj) + 1);
}

static inline int twz_bus_open_child(struct object *bus,
  struct object *ch,
  size_t num,
  uint32_t flags)
{
	struct bus_repr *br = twz_bus_getrepr(bus);
	struct kso_attachment *ids = twz_ptr_lea(bus, br->children);
	if(!ids[num].id)
		return -ENOENT;
	return twz_object_open(ch, ids[num].id, flags);
}

#endif
