#pragma once

/* support for SMALL transactions. Larger transactions need to be done with a copy + atomic-swap
 * scheme */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct __tx_log_entry {
	void *ptr;
	uint32_t len;
	uint32_t next;
	char value[];
};

struct twz_tx {
	size_t logsz;
	size_t end;
	char log[];
};

#include <setjmp.h>
#include <stdio.h>
#include <twz/obj.h>
#include <twz/persist.h>

static inline int __tx_add(struct twz_tx *tx, void *p, size_t len)
{
	if(tx->end + sizeof(struct __tx_log_entry) + len > tx->logsz)
		return -ENOMEM;
	struct __tx_log_entry *entry = (void *)&tx->log[tx->end];
	entry->ptr = twz_ptr_local(p);
	entry->len = len;
	memcpy(entry->value, p, len);
	size_t next = sizeof(*entry) + len;
	next = (next + 7) & ~7;
	entry->next = next;
	_clwb_len(entry, sizeof(*entry) + len);
	_pfence();

	tx->end += next;
	_clwb(&tx->end);
	_pfence();
	return 0;
}

static inline int __tx_cleanup(twzobj *obj, struct twz_tx *tx, bool abort)
{
	size_t e = 0;
	while(e < tx->end) {
		struct __tx_log_entry *entry = (void *)&tx->log[e];
		void *vp = twz_object_lea(obj, entry->ptr);
		if(abort) {
			memcpy(vp, entry->value, entry->len);
		}
		_clwb_len(vp, entry->len);
		e += entry->next;
	}

	if(tx->end) {
		_pfence();
		tx->end = 0;
		_clwb(&tx->end);
		_pfence();
	}
}

static inline int __tx_commit(twzobj *obj, struct twz_tx *tx)
{
	return __tx_cleanup(obj, tx, false);
}

static inline int __tx_abort(twzobj *obj, struct twz_tx *tx)
{
	return __tx_cleanup(obj, tx, true);
}

#include <assert.h>

#define __TX_NORMAL 0
#define __TX_SUCCESS 1
#define __TX_ABORT 2

#define TXCHECK(tx) __tx_abort(tx)

#define TXSTART(obj, tx)                                                                           \
	{                                                                                              \
		__tx_abort((obj), (tx));                                                                   \
		jmp_buf _env;                                                                              \
		int _r, _code = 0, _type = __TX_NORMAL;                                                    \
		_r = setjmp(_env);                                                                         \
		if(_r) {                                                                                   \
			_code = _r >> 8;                                                                       \
			_type = _r & 0xf;                                                                      \
			__tx_cleanup((obj), (tx), _type == __TX_ABORT);                                        \
		}                                                                                          \
		switch(_type) {                                                                            \
			case __TX_NORMAL:

#define TXEND                                                                                      \
	break;                                                                                         \
	}                                                                                              \
	assert(_type != __TX_NORMAL);                                                                  \
	errno = _code;                                                                                 \
	}

#define TX_ONABORT                                                                                 \
	break;                                                                                         \
	case __TX_ABORT:                                                                               \
		errno = _code;

#define TX_ONSUCCESS                                                                               \
	break;                                                                                         \
	case __TX_SUCCESS:

#define TXABORT(code) longjmp(_env, ((code) << 8) | __TX_ABORT)

#define TXCOMMIT longjmp(_env, __TX_SUCCESS)

#define TXRECORD_LEN(tx, x, l)                                                                     \
	({                                                                                             \
		int _rr = __tx_add((tx), (x), (l));                                                        \
		if(_rr)                                                                                    \
			longjmp(_env, (_rr << 8) | __TX_ABORT);                                                \
	})

#define TXRECORD(tx, x) TXRECORD_LEN((tx), (x), sizeof(*(x)))
