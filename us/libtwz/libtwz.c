#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static bool disable_backtrace = false;

__attribute__((noreturn)) void libtwz_panic(const char *s, ...)
{
	va_list va;
	va_start(va, s);
	vfprintf(stderr, s, va);
	if(s[strlen(s)] != '\n')
		fprintf(stderr, "\n");
	va_end(va);
	libtwz_do_backtrace();
	disable_backtrace = true;
	abort();
}

#if __has_include(<backtrace.h>)
#include <backtrace-supported.h>
#include <backtrace.h>

__attribute__((weak)) extern struct backtrace_state *backtrace_create_state(const char *filename,
  int threaded,
  backtrace_error_callback error_callback,
  void *data);

__attribute__((weak)) extern void backtrace_print(struct backtrace_state *state, int skip, FILE *);

__attribute__((weak)) extern int backtrace_full(struct backtrace_state *state,
  int skip,
  backtrace_full_callback callback,
  backtrace_error_callback error_callback,
  void *data);

__attribute__((weak)) extern int backtrace_syminfo(struct backtrace_state *state,
  uintptr_t addr,
  backtrace_syminfo_callback callback,
  backtrace_error_callback error_callback,
  void *data);

struct bt_ctx {
	struct backtrace_state *state;
	int error;
	int count;
};

static void __print_backtrace_line(int frnr,
  unsigned long pc,
  const char *filename,
  size_t line,
  const char *function)
{
	fprintf(stderr,
	  "#%-3d \e[34m%18lx \e[33m%-24s \e[32m%s\e[0m:\e[36m%ld\e[0m\n",
	  frnr,
	  (unsigned long)pc,
	  function,
	  filename,
	  line);
}

static void syminfo_callback(void *data,
  uintptr_t pc,
  const char *symname,
  uintptr_t symval,
  uintptr_t symsize)
{
	struct bt_ctx *ctx = data;
	__print_backtrace_line(ctx->count, (unsigned long)pc, "??", 0, symname ? symname : "??");
}

static void error_callback(void *data, const char *msg, int errnum)
{
	struct bt_ctx *ctx = data;
	fprintf(stderr, "backtrace ERROR: %s (%d)\n", msg, errnum);
	ctx->error = 1;
}

static int full_callback(void *data,
  uintptr_t pc,
  const char *filename,
  int lineno,
  const char *function)
{
	struct bt_ctx *ctx = data;
	if(function) {
		__print_backtrace_line(
		  ctx->count, (unsigned long)pc, filename ? filename : "??", lineno, function);
	} else {
		backtrace_syminfo(ctx->state, pc, syminfo_callback, error_callback, data);
	}
	ctx->count++;
	return 0;
}

__attribute__((noinline)) void libtwz_do_backtrace(void)
{
	if(disable_backtrace)
		return;
	if(backtrace_create_state) {
		struct backtrace_state *state =
		  backtrace_create_state(NULL, BACKTRACE_SUPPORTS_THREADS, error_callback, NULL);
		struct bt_ctx ctx = { state, 0, 0 };
		backtrace_full(state, 1, full_callback, error_callback, &ctx);
	} else {
		fprintf(stderr,
		  "BACKTRACE NOT AVAILABLE -- if you want backtraces, specify `-Wl,--whole-archive "
		  "-lbacktrace "
		  "-Wl,--no-whole-archive' on the linker command line.\n");
	}
}

#else

__attribute__((noinline)) void libtwz_do_backtrace(void)
{
	if(disable_backtrace)
		return;
	fprintf(stderr,
	  "BACKTRACE NOT AVAILABLE -- this libtwz was compiled without backtrace.h in sysroot. If you "
	  "want backtraces, please port libbacktrace (through the ports system) and then recompile "
	  "libtwz.\n");
}

#endif
