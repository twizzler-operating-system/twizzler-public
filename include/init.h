#pragma once
#include <system.h>
struct init_call {
	void (*fn)(void *);
	void *data;
	struct init_call *next;
	bool allcpus;
};

#define __late_init_arg(c, f,a) \
	static inline void __initializer __concat(__reg_post_init, __COUNTER__) (void) { post_init_call_register(c, f, a); }

#define __late_init(c,f) __late_init_arg(c, f, NULL)

#define __late_init_arg_ordered(c,p,f,a) \
	static inline void __orderedinitializer(p) __concat(__reg_post_init, __COUNTER__) (void) { post_init_call_register(c, f, a); }

#define __late_init_ordered(c,p,f) __late_init_arg_ordered(c, p, f, NULL)


#define POST_INIT(...) __get_macro2(__VA_ARGS__,__late_init_arg,__late_init)(false, __VA_ARGS__)
#define POST_INIT_ORDERED(pri, ...) __get_macro2(__VA_ARGS__,__late_init_arg_ordered,__late_init_ordered)(false, pri, __VA_ARGS__)

#define POST_INIT_ALLCPUS(...) __get_macro2(__VA_ARGS__,__late_init_arg,__late_init)(true, __VA_ARGS__)
#define POST_INIT_ORDERED_ALLCPUS(pri, ...) __get_macro2(__VA_ARGS__,__late_init_arg_ordered,__late_init_ordered)(true, pri, __VA_ARGS__)


void post_init_call_register(bool allc, void (*fn)(void *), void *data);

