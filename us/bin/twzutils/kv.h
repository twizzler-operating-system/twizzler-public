#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct twzkv_item {
	void *data;
	size_t length;
};

int twzkv_get(struct twzkv_item *key, struct twzkv_item *value);
int twzkv_put(struct twzkv_item *key, struct twzkv_item *value);
#include <twz/obj.h>
void init_database(twzobj *, twzobj *);
