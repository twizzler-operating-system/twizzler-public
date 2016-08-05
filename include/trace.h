#pragma once

#include <stdarg.h>
#include <system.h>

struct trace {
	bool enabled;
	const char *name;
};

#define TRACE_INITIALIZER(n,e) (struct trace) { .enabled = e, .name = n }
#if CONFIG_DEBUG

static inline bool __trace(struct trace *trace, const char *msg, ...)
{
	if(!trace->enabled)
		return false;
	va_list args;
	va_start(args, msg);
	vprintk(msg, args);
	va_end(args);
	return true;
}

extern int trace_indent_level;

struct trace_node {
	struct trace *trace;
	bool did;
	bool cleanup;
	const char *fn;
};

static inline void __trace_cleanup(struct trace_node *node)
{
	if(!node->cleanup || !node->did)
		return;
	printk("%*s[%s] %s: function returned\n", --trace_indent_level, "", node->trace->name, node->fn);
}

#define TRACE(t,f,m,...) \
	__cleanup(__trace_cleanup) struct trace_node __concat(__trace_node, __COUNTER__) = {.did = __trace(t, "%*s[%s] %s: " m "\n", f ? trace_indent_level++ : trace_indent_level, "", (t)->name, __FUNCTION__, ##__VA_ARGS__), .cleanup = f, .trace = (t), .fn = __FUNCTION__ }

#define TRACEFN(t,m,...) \
	TRACE(t, true, m, ##__VA_ARGS__)

#else

#define TRACE(t,m,...)

#endif

