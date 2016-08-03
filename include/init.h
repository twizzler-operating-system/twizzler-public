#pragma once
#include <system.h>
struct init_call {
	void (*fn)(void *);
	void *data;
	struct init_call *next;
};

#define __late_init_arg(f,a) \
	static inline void __initializer __concat(__reg_post_init, __COUNTER__) (void) { post_init_call_register(f, a); }

#define __late_init(f) __late_init_arg(f, NULL)

#define __late_init_arg_ordered(p,f,a) \
	static inline void __orderedinitializer(p) __concat(__reg_post_init, __COUNTER__) (void) { post_init_call_register(f, a); }

#define __late_init_ordered(p,f) __late_init_arg_ordered(p, f, NULL)


#define POST_INIT(...) __get_macro2(__VA_ARGS__,__late_init_arg,__late_init)(__VA_ARGS__)
#define POST_INIT_ORDERED(pri, ...) __get_macro2(__VA_ARGS__,__late_init_arg_ordered,__late_init_ordered)(pri, __VA_ARGS__)

void post_init_call_register(void (*fn)(void *), void *data);

