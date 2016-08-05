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
	/* if we did the trace and we're exiting a scope, print a message */
	if(!node->cleanup || !node->did)
		return;
	printk("%*s[%s] %s: function returned\n", --trace_indent_level, "", node->trace->name, node->fn);
}

/* usage: TRACE(trace-name, tracing-scope, printf-like-message, ...)
 * trace-name is a pointer to a struct trace, so that sets of traces may be disabed or enabled at runtime.
 * tracing-scope, if false, will simply print a trace message at the current indent level.
 *                if true,  will print a message when the scope ends as well, and will increment indent level.
 * This lets you do stuff like:
 * void foo() { TRACE(&t, true, "test! %d", 4); ... ; TRACE(&t, false, "something happened"); }
 * which will print out:
 * [trace] foo: test! 4
 *  [trace] foo: something happened
 * [trace] foo: function returned
 */
#define TRACE(t,f,m,...) \
	__cleanup(__trace_cleanup) struct trace_node __concat(__trace_node, __COUNTER__) = { /* create a new unique-named node that will destruct with __trace_cleanup */ \
		.did = __trace(t, "%*s[%s] %s: " m "\n", /* format string includes an indentation, trace name, function name, message, and newline */ \
				f ? trace_indent_level++ : trace_indent_level, /* increment only if we're entering a new scope */ \
				"", (t)->name, __FUNCTION__, ##__VA_ARGS__),\
		.cleanup = f, .trace = (t), .fn = __FUNCTION__\
	}

#define TRACEFN(t,m,...) \
	TRACE(t, true, m, ##__VA_ARGS__)

#else

#define TRACE(t,m,...)

#endif

