#pragma once

#define foreach(e, type, container) \
	for(struct type *e = __concat(type, _iter_start)(container); \
			e != __concat(type, _iter_end)(container); \
			e = __concat(type, _iter_next)(e))
