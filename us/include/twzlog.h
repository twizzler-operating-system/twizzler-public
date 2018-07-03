#pragma once

enum {
	TWZLOG_EMERG,
	TWZLOG_ALERT,
	TWZLOG_ERROR,
	TWZLOG_WARN,
	TWZLOG_NOTICE,
	TWZLOG_INFO,
	TWZLOG_DEBUG,
};

typedef struct twzlog {
	const char *logident;
	unsigned logflags;
	struct object lo;
} twzlog;

void twzlog_open(twzlog *, const char *ident, unsigned flags);
void twzlog_close(twzlog *);
void twzlog_write(twzlog *, int pri, const char *msg, ...);


