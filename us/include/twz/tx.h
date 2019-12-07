#pragma once

/* support for SMALL transactions. Larger transactions need to be done with a copy + atomic-swap
 * scheme */

#include <stdint.h>

struct __tx_log_entry {
	void *ptr;
	uint16_t len;
	uint16_t next;
	char value[];
};

#include <twz/persist.h>

struct twz_tx {
	uint32_t logsz;
	uint32_t tmpend;
	uint8_t pad0[__CL_SIZE - 8];
	uint32_t end;
	uint8_t pad1[__CL_SIZE - 4];
	char log[];
};

#include <errno.h>
#include <setjmp.h>
#include <twz/obj.h>

static void tx_init(struct twz_tx *tx, uint32_t logsz)
{
	tx->logsz = logsz;
	tx->tmpend = tx->end = 0;
	_clwb(&tx->end);
	_pfence();
}

static inline int __tx_add_noflush(struct twz_tx *tx, void *p, uint16_t len)
{
	if(tx->tmpend + sizeof(struct __tx_log_entry) + len > tx->logsz)
		return -ENOMEM;
	struct __tx_log_entry *entry = (void *)&tx->log[tx->tmpend];
	entry->ptr = twz_ptr_local(p);
	entry->len = len;
	memcpy(entry->value, p, len);
	uint16_t next = sizeof(*entry) + len;
	next = (next + 7) & ~7;
	entry->next = next;

	tx->tmpend += next;
	return 0;
}

static inline int __tx_add_commit(struct twz_tx *tx)
{
	_clwb_len(tx->log, tx->tmpend);
	_pfence();
	tx->end = tx->tmpend;
	_clwb(&tx->end);
	_pfence();
}

static inline int __tx_add(struct twz_tx *tx, void *p, uint16_t len)
{
	if(tx->tmpend + sizeof(struct __tx_log_entry) + len > tx->logsz)
		return -ENOMEM;
	struct __tx_log_entry *entry = (void *)&tx->log[tx->tmpend];
	entry->ptr = twz_ptr_local(p);
	entry->len = len;
	memcpy(entry->value, p, len);
	uint16_t next = sizeof(*entry) + len;
	next = (next + 7) & ~7;
	entry->next = next;
	_clwb_len(entry, sizeof(*entry) + len);
	_pfence();

	tx->end += next;
	tx->tmpend += next;
	_clwb(&tx->end);
	_pfence();
	return 0;
}

static inline int __same_line(void *a, void *b)
{
	return ((uintptr_t)a & ~(__CL_SIZE - 1)) == ((uintptr_t)b & ~(__CL_SIZE - 1));
}

static inline int __tx_cleanup(twzobj *obj, struct twz_tx *tx, _Bool abort)
{
	uint32_t e = 0;
	void *last_vp = NULL;
	long long last_len = 0;
	while(e < tx->end) {
		struct __tx_log_entry *entry = (void *)&tx->log[e];
		void *vp = twz_object_lea(obj, entry->ptr);
		if(abort) {
			memcpy(vp, entry->value, entry->len);
		}

#if 1
		/* TODO: verify correctness */
		char *l = vp;
		char *last_l = last_vp;
		long long rem = entry->len;
		while(rem > 0) {
			if(last_len == 0 || !__same_line(last_l, l))
				_clwb(l);
			uint32_t off = (uintptr_t)l & (__CL_SIZE - 1);
			uint32_t last_off = (uintptr_t)last_l & (__CL_SIZE - 1);
			l += (__CL_SIZE - off);
			last_l += (__CL_SIZE - off);
			rem -= (__CL_SIZE - off);
			last_len -= (__CL_SIZE - last_off);
		}

		last_len = entry->len;
		last_vp = vp;
#else
		_clwb_len(vp, entry->len);
#endif
		e += entry->next;
	}

	if(tx->end) {
		_pfence();
		tx->end = 0;
		_clwb(&tx->end);
		_pfence();
	}
	tx->tmpend = 0;
}

static inline int __tx_commit(twzobj *obj, struct twz_tx *tx)
{
	return __tx_cleanup(obj, tx, 0);
}

static inline int __tx_abort(twzobj *obj, struct twz_tx *tx)
{
	return __tx_cleanup(obj, tx, 1);
}

#include <assert.h>

#define __TX_NORMAL 0
#define __TX_SUCCESS 1
#define __TX_ABORT 2

#define TXOPT_START(obj, tx, rcode)                                                                \
	do {                                                                                           \
		int _type = __TX_NORMAL;                                                                   \
		int _code = 0;                                                                             \
		int _done = 0;                                                                             \
		while(!_done) {                                                                            \
			__tx_cleanup((obj), (tx), _type != __TX_SUCCESS);                                      \
			switch(_type) {                                                                        \
				default:                                                                           \
					_done = 1;                                                                     \
					break;                                                                         \
				case __TX_NORMAL:

#define TXOPT_END                                                                                  \
	break;                                                                                         \
	}                                                                                              \
	}                                                                                              \
	rcode = (_code << 8) | _type;                                                                  \
	}                                                                                              \
	while(0)

#define TXOPT_ABORT(code)                                                                          \
	_type = __TX_ABORT;                                                                            \
	_code = code;                                                                                  \
	break

#define TXOPT_COMMIT                                                                               \
	_type = __TX_SUCCESS;                                                                          \
	break

#define TXOPT_RECORD_LEN(tx, x, l)                                                                 \
	({                                                                                             \
		int _rr = __tx_add((tx), (x), (l));                                                        \
		if(_rr) {                                                                                  \
			_code = _rr;                                                                           \
			_type = __TX_ABORT;                                                                    \
			break;                                                                                 \
		}                                                                                          \
	})

#define TXOPT_RECORD_LEN_TMP(tx, x, l)                                                             \
	({                                                                                             \
		int _rr = __tx_add_noflush((tx), (x), (l));                                                \
		if(_rr) {                                                                                  \
			_code = _rr;                                                                           \
			_type = __TX_ABORT;                                                                    \
			break;                                                                                 \
		}                                                                                          \
	})

#define TXOPT_RECORD(tx, x) TXOPT_RECORD_LEN((tx), (x), sizeof(*(x)))

#define TXOPT_RECORD_TMP(tx, x) TXOPT_RECORD_LEN_TMP((tx), (x), sizeof(*(x)))

#define TX_RECORD_COMMIT(tx) __tx_add_commit(tx)

#define TXCHECK(obj, tx) __tx_abort((obj), (tx))

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

#define TXRECORD_LEN_TMP(tx, x, l)                                                                 \
	({                                                                                             \
		int _rr = __tx_add_noflush((tx), (x), (l));                                                \
		if(_rr)                                                                                    \
			longjmp(_env, (_rr << 8) | __TX_ABORT);                                                \
	})

#define TXRECORD(tx, x) TXRECORD_LEN((tx), (x), sizeof(*(x)))

#define TXRECORD_TMP(tx, x) TXRECORD_LEN_TMP((tx), (x), sizeof(*(x)))
