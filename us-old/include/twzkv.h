#pragma once

struct twzkv_item {
	void *data;
	size_t length;
};

struct object;
int twzkv_get(struct object *index, struct twzkv_item *key, struct twzkv_item *value);
int twzkv_put(struct object *index, struct object *data,
		struct twzkv_item *key, struct twzkv_item *value);
int twzkv_create_index(struct object *index);
void twzkv_init_index(struct object *index);
int twzkv_create_data(struct object *data);
void twzkv_init_data(struct object *data);
int twzkv_foreach(struct object *index, void (*fn)(struct twzkv_item *k, struct twzkv_item *v));

