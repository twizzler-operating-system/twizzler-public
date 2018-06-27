#pragma once
extern objid_t kc_init_id, kc_bsv_id;

void kc_parse(const char *data, size_t len);

bool objid_parse(const char *name, objid_t *id);
