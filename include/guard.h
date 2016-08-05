#pragma once

/* USAGE:
 * defer(func) or defer(func, arg).
 *   used for deferred cleanup. For example,
 *     char *foo = malloc(size);
 *     defer(free, foo);
 *     ... use foo ...
 *     return;
 *   will cause free(foo) to be called when foo goes out of scope.
 *
 * guard(m,a,r).
 *   used for lock-guards. For example,
 *     struct mutex m;
 *     ...
 *     {
 *       guard(m,mutex_acquire,mutex_release);
 *       ...
 *     }
 *   The mutex will be automatically released when the scope that the
 *   guard was declared in is destroyed.
 * NOTES:
 * These are basically destructors for variables, a la C++. Use them carefully,
 * as they do not fit the imperative programming style particularly well. They
 * create invisible function calls which may be confusing. On the other hand,
 * they are very useful and can make complicated functions much simpler.
 */

#include <system.h>
struct _guard {
	void *data;
	void (*fn)();
};

static inline void _guard_release(struct _guard *g)
{
	g->fn(g->data);
}

/* this macro does two things: it calls 'a', and acts as the assignment part of a variable declaration + assignment.
 * It initializes a struct _guard with data <- l and fn <- r. It is specified to be destructed with a call to
 * _guard_release when it goes out of scope. _guard_release then calls fn(data).
 *
 * This can be used to implement lock-guards and deferred function calls (useful for profiling, tracing, and cleanup).
 */

#define guard_initializer(l,a,r) \
	__cleanup(_guard_release) = (struct _guard) { .data = ((a != NULL ? ((void (*)())a)(l) : 0), l), .fn = r }

#define guard_initializer2(l,a,r) \
	__cleanup(_guard_release) = (struct _guard) { .data = (((void (*)())a)(l), l), .fn = r }

/* create a guard with a unique name. */
#define guard(m,a,r) struct _guard __concat(_lg_, __COUNTER__) guard_initializer(m,a,r)
#define guard2(m,a,r) struct _guard __concat(_lg_, __COUNTER__) guard_initializer2(m,a,r)

/* fancy way to overload macros in C. End result is I can call defer(func) or defer(func, arg) and both
 * will work. In the first case, func will be called with no arguments when a scope is destoyed, and in the
 * second case it will be called with arg as its only argument.
 */
#define __defer(f) guard(NULL, NULL, f)
#define __defer_arg(f,a) guard(a, NULL, f)

/* defer a function call until a scope is destroyed. */
#define defer(...) __get_macro2(__VA_ARGS__,__defer_arg,__defer)(__VA_ARGS__)

