Data Structure Consistency (us/consistency.md)
==========================

Twizzler provides a basic set of consistency primitives, under the hope that future language support
will provide better consistency operations (much like what happened with atomics and threading!).

Detailed documentation on this is pending, but the basics are...

See us/include/twz/persist.h for the following functions:

 * `_clwb(void *)`: Cache-line writeback a cache-line.
 * `_clwb_len(void *, size_t)` Writeback a region of memory.
 * `_pfence()`: Issue a fence to ensure consistency.

Transaction Logging
-------------------

See us/include/twz/tx.h for a basic transactions system implemented as an undo log. NOTE: these
transactions are not for coherence! They only work in single-threaded code, so if you want to use
these in a multithreaded environment, put a mutex around it.

Your object needs to have a `struct twz_tx` object. This can be initialized with `twz_init(struct twz_tx *tx, size_t loglen);`

A transaction looks like this:


```{.c}
struct header *hdr = /*something*/;
TXSTART(obj, &hdr->tx) {
	TXRECORD(&hdr->tx, hdr->foo);
	hdr->foo = 3;
	if(want_to_commit)
		TXCOMMIT;
	else
		TXABORT(err_code);
} TX_ONABORT {
	/* do something with errno */
} TX_ONSUCCESS {
	/* we did it */
} TXEND;
```

The `TX_ONABORT` and `TX_ONSUCCESS` bits are optional.

Whenever you access transaction-protected variables outside of a transaction, you must first ensure
that there is no pending transaction abort. You can do this with `TXCHECK(obj, tx)`.

