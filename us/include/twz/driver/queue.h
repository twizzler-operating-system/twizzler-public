#pragma once

#define __QUEUE_TYPES_ONLY
#include <twz/_queue.h>

struct queue_entry_pager {
	struct queue_entry qe;
	objid_t id;
	objid_t reqthread;
	unsigned long tid;
	size_t page;
	size_t length;
	uint64_t linaddr;
};

#define PAGER_CMD_OBJECT 1
#define PAGER_CMD_OBJECT_PAGE 2
