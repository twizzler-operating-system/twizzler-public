#include <twz.h>
#include <twzobj.h>
#include <twzio.h>
#include <stdio.h>
#include <string.h>
#include <twzlog.h>
#include <stddef.h>
#include <bstream.h>
#include <stdarg.h>
#include <twzicall.h>

void twzlog_open(twzlog *tw, const char *ident, unsigned flags)
{
	objid_t bsid = 0;
	if(tw->logident) return;
	if(twz_object_new(&tw->lo, &bsid, 0, /* TODO: kuid */ 0, 0)) return;
	
	bstream_init(&tw->lo, 12);

	//struct icall ic;
	//twz_icall_init(&ic, "view:twzlogd", "twzlogd_watch");
	//if(twz_icall(&ic, ID_LO(bsid), ID_HI(bsid), 0, 0, 0, 0) != 0) return;

	tw->logflags = flags;
	tw->logident = ident;
}

void twzlog_close(twzlog *tw)
{
	tw->logident = NULL;
	/* TODO: destroy object */
}

void twzlog_write(twzlog *tw, int pr, const char *msg, ...)
{
	if(!tw->logident) return;
	va_list va;
	va_start(va, msg);

	char buffer[1024];
	size_t n = snprintf(buffer, 512, "[%d:%s] ", pr, tw->logident);
	size_t n2 = vsnprintf(buffer+n, 1024-n, msg, va);
	va_end(va);

	bstream_write(&tw->lo, (unsigned char *)buffer, n+n2, 0);
}

