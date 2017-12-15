#include <ksymbol.h>
#include <debug.h>
#include <lib/inthash.h>
#include <string.h>
#include <processor.h>
#include <lib/lib.h>

#define INSTRUMENT_TABLE_LEN 1024

struct tracepoint {
	void *fn;
	void *caller;
	long long start;
};

struct tracefn {
	uintptr_t key;
	struct ihelem elem;
	unsigned long count, mean, S, max, min;
};

static struct tracefn fntraces[INSTRUMENT_TABLE_LEN];
static int fntraces_ind = 0;

DECLARE_IHTABLE(tracetable, 8);

/* TODO: percpu */
static struct tracepoint function_trace_stack[1024] = {NULL};
static int fts_ind = 0;

static bool disable_trace = 0;

__noinstrument
void __cyg_profile_func_enter(void *func, void *caller)
{
	if(disable_trace) return;
	disable_trace = true;
	if(unlikely(kernel_symbol_table_length >= INSTRUMENT_TABLE_LEN)) {
		panic("--> increase INSTRUMENT_TABLE_LEN");
	}
	if(fts_ind == 1024) {
		panic("function_trace_stack exceeded");
	}
	if(function_trace_stack[fts_ind].fn == func) {
		panic("direct recusive call detected");
	}
	for(int i=0;i<fts_ind;i++) {
		if(function_trace_stack[i].fn == func) {
			disable_trace = false;
			return;
		}
	}
	function_trace_stack[fts_ind].fn = func;
	function_trace_stack[fts_ind].caller = caller;
	disable_trace = false;
	function_trace_stack[fts_ind++].start = arch_processor_timestamp();
}

__noinstrument
void __cyg_profile_func_exit(void *func, void *caller __unused)
{
	unsigned long long ts = arch_processor_timestamp();
	if(disable_trace) return;
	disable_trace = true;
	if(fts_ind == 0) {
		panic("function_trace_stack underflow");
	}
	if(function_trace_stack[fts_ind-1].fn == func) {
		unsigned long long diff = ts - function_trace_stack[fts_ind-1].start;
		const struct ksymbol *ks = ksymbol_find_by_value((uintptr_t)func, true);
		if(ks) {
			struct tracefn *tf = ihtable_find(&tracetable, (uintptr_t)ks,
					struct tracefn, elem, key);
			if(tf == NULL) {
				if(fntraces_ind >= INSTRUMENT_TABLE_LEN) {
					panic("instrumentation table exhausted");
				}
				tf = &fntraces[fntraces_ind++];
				tf->key = (uintptr_t)ks;
				tf->count = 0;
				tf->mean = 0;
				tf->min = ~0;
				tf->max = 0;
				tf->S = 0;
				ihtable_insert(&tracetable, &tf->elem, tf->key);
			}
			int x = tf->count++;
			unsigned long long prev_mean = tf->mean;
			if(x) {
				if(diff > tf->max) tf->max = diff;
				if(diff < tf->min) tf->min = diff;
			}
			tf->mean = ((1000ul*((prev_mean * x) + diff)) / (x+1))/1000ul;
			tf->S += (diff - tf->mean) * (diff - prev_mean);
		}
		function_trace_stack[--fts_ind].fn = NULL;
	}
	disable_trace = false;
}

static int __compar(const void *_a, const void *_b)
{
	const struct tracefn *a = _a;
	const struct tracefn *b = _b;
	return a->count - b->count;
}

static void __calibrate_instrumentation(void);
__attribute__((noinline))
static void __calibration_amount(void)
{
	__cyg_profile_func_enter(__calibration_amount, __calibrate_instrumentation);
	__cyg_profile_func_exit(__calibration_amount, __calibrate_instrumentation);
}

__initializer
static void __calibrate_instrumentation(void)
{
	printk("calibrating instrumentation system\n");
	for(int i=0;i<100000;i++) __calibration_amount();
}

static struct tracefn sorted_traces[INSTRUMENT_TABLE_LEN];
static struct spinlock results_lock = SPINLOCK_INIT;
void instrument_print_results(void)
{
	bool slflag = spinlock_acquire(&results_lock);
	disable_trace = true;
	memcpy(sorted_traces, fntraces, sizeof(sorted_traces));
	qsort(sorted_traces, fntraces_ind, sizeof(struct tracefn), __compar);

	long total_count = 0;
	for(int i=0;i<fntraces_ind;i++) {
		struct tracefn *tf = &sorted_traces[i];
		struct ksymbol *ks = (struct ksymbol *)tf->key;
		total_count += tf->count;
		if(tf->count > 50)
			printk(":: %8.0ld: %8.0ld (%6.0d): %6.0ld: %8.0ld: %8.0ldK: %s\n", tf->count, tf->mean,
					isqrt(tf->S / tf->count), tf->min, tf->max,
					tf->count*tf->mean / 1000, ks->name);
	}
	printk("--- total count: %ld\n", total_count);
	printk("--- fntraces_ind: %d\n", fntraces_ind);
	disable_trace = false;
	spinlock_release(&results_lock, slflag);
}

