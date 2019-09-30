#pragma once
struct proc_stats {
	_Atomic uint64_t thr_switch;
	_Atomic uint64_t syscalls;
	_Atomic uint64_t sctx_switch;
	_Atomic uint64_t ext_intr;
	_Atomic uint64_t int_intr;
	_Atomic uint64_t running;
	_Atomic uint64_t shootdowns;
};

struct processor_header {
	struct proc_stats stats;
};
