#pragma once

struct twzkv_item {
	void *data;
	size_t length;
};

int twzkv_get(struct twzkv_item *key, struct twzkv_item *value);
int twzkv_put(struct twzkv_item *key, struct twzkv_item *value);
void twzkv_init_database(void);

