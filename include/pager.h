#pragma once

#include <object.h>
#include <stdint.h>

int kernel_queue_pager_request_object(objid_t id);
int kernel_queue_pager_request_page(struct object *obj, size_t pg);
void pager_idle_task(void);
