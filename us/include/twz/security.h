#pragma once

#include <twz/_sctx.h>

int twz_cap_create(struct sccap **cap,
  objid_t target,
  objid_t accessor,
  uint32_t perms,
  struct screvoc *revoc,
  struct scgates *gates,
  uint16_t htype,
  uint16_t etype);

int twz_dlg_create(struct scdlg **dlg,
  void *item,
  size_t itemlen,
  objid_t delegator,
  uint32_t mask,
  struct screvoc *revoc,
  struct scgates *gates,
  uint16_t htype,
  uint16_t etype);

struct object;
int twz_sctx_add(struct object *obj, objid_t target, void *item, size_t itemlen);
ssize_t twz_sctx_lookup(struct object *obj, objid_t target);
ssize_t twz_sctx_next(struct object *obj, objid_t target, ssize_t bucket);
int twz_sctx_read(struct object *obj, objid_t target, void *data, size_t *datalen);
int twz_sctx_del(struct object *obj, objid_t target, ssize_t bucketnum);
