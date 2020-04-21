#include <debug.h>
#include <init.h>
#include <ksymbol.h>
#include <lib/inthash.h>
#include <lib/lib.h>
#include <processor.h>
#include <slab.h>
#include <string.h>
#define INSTRUMENT_TABLE_LEN 2048

struct tracepoint {
	void *fn;
	void *caller;
	unsigned long long start, tt;
};

_Atomic int disable_trace = 1;
static DECLARE_PER_CPU(struct tracepoint, function_trace_stack)[64] = { NULL };
static DECLARE_PER_CPU(_Atomic int, disable_trace_local) = 0;
static DECLARE_PER_CPU(int, fts_ind) = 0;
static DECLARE_PER_CPU(long long, time_tracing) = 0;

__noinstrument void instrument_disable(void)
{
	if(disable_trace)
		return;
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	(*disable_trace_local)++;
}

__noinstrument void instrument_enable(void)
{
	if(disable_trace)
		return;
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	(*disable_trace_local)--;
}

__noinstrument bool instrument_disabled(void)
{
	if(disable_trace)
		return true;
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	return *disable_trace_local != 0;
}

#if 1
__noinstrument void __cyg_profile_func_enter(void *func, void *caller)
{
	unsigned long long ts = arch_processor_timestamp();
	if(instrument_disabled()) {
		if(!disable_trace) {
			// long long *tt = per_cpu_get(time_tracing);
			//*tt += arch_processor_timestamp() - ts;
		}
		return;
	}
	int *fi = per_cpu_get(fts_ind);
	long long *tt = per_cpu_get(time_tracing);
	instrument_disable();
	const struct ksymbol *ks = ksymbol_find_by_value((uintptr_t)func, true);
	if(ks) {
		*fi++;
		printk(
		  "{ instrtrace enter %d %s %p %p %lld %lld }\n", *fi, ks->name, func, caller, ts, *tt);
	}

	instrument_enable();
	*tt += arch_processor_timestamp() - ts;
}

__noinstrument void __cyg_profile_func_exit(void *func, void *caller __unused)
{
	unsigned long long ts = arch_processor_timestamp();
	if(instrument_disabled()) {
		if(!disable_trace) {
			// long long *tt = per_cpu_get(time_tracing);
			//*tt += arch_processor_timestamp() - ts;
		}
		return;
	}
	int *fi = per_cpu_get(fts_ind);
	long long *tt = per_cpu_get(time_tracing);
	instrument_disable();

	const struct ksymbol *ks = ksymbol_find_by_value((uintptr_t)func, true);
	if(ks) {
		*fi--;
		printk("{ instrtrace exit %d %s %p %p %lld %lld }\n", *fi, ks->name, func, caller, ts, *tt);
	}

	instrument_enable();
	*tt += arch_processor_timestamp() - ts;
}

#else

__noinstrument void __cyg_profile_func_enter(void *func, void *caller)
{
	if(instrument_disabled())
		return;
	unsigned long long ts = arch_processor_timestamp();
	long long *tt = per_cpu_get(time_tracing);
	instrument_disable();
	if(unlikely(kernel_symbol_table_length >= INSTRUMENT_TABLE_LEN)) {
		panic("--> increase INSTRUMENT_TABLE_LEN");
	}
	// instrument_enable();
	// return;
	int *fi = per_cpu_get(fts_ind);
	if(*fi == 64) {
		panic("function_trace_stack exceeded");
	}
	struct tracepoint *function_trace_stack = *per_cpu_get(function_trace_stack);
	if(function_trace_stack[*fi].fn == func) {
		panic("direct recusive call detected :: %p %p", func, caller);
	}
	function_trace_stack[*fi].fn = func;
	function_trace_stack[*fi].caller = caller;
	instrument_enable();
	function_trace_stack[*fi].tt = *tt;
	*tt += arch_processor_timestamp() - ts;
	function_trace_stack[(*fi)++].start = arch_processor_timestamp();
}

__noinstrument void __cyg_profile_func_exit(void *func, void *caller __unused)
{
	unsigned long long ts = arch_processor_timestamp();
	if(instrument_disabled())
		return;
	long long *tt = per_cpu_get(time_tracing);
	// return;
	instrument_disable();
	// instrument_enable();
	// return;
	int *fi = per_cpu_get(fts_ind);
	if(*fi == 0) {
		instrument_enable();
		*tt += arch_processor_timestamp() - ts;
		return;
	}
	struct tracepoint *function_trace_stack = *per_cpu_get(function_trace_stack);
	if(function_trace_stack[*fi - 1].fn == func) {
		const struct ksymbol *ks = ksymbol_find_by_value((uintptr_t)func, true);
		if(ks) {
			printk("{instrtrace %d %d %p %p %s %lld %lld %lld}\n",
			  current_processor->id,
			  *fi,
			  func,
			  caller,
			  ks->name,
			  function_trace_stack[*fi - 1].start,
			  ts,
			  *tt - function_trace_stack[*fi - 1].tt);
		}
		function_trace_stack[--(*fi)].fn = NULL;
	}
	instrument_enable();
	*tt += arch_processor_timestamp() - ts;
}
#endif

void kernel_instrument_start(void)
{
	disable_trace = 0;
}

static void __init_instrumentation(void *_a __unused)
{
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	(*disable_trace_local) = 0;
}
POST_INIT_ALLCPUS(__init_instrumentation);
