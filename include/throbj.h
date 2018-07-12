#pragma once

struct twzthread_repr {
	union {
		struct {
			struct faultinfo faults[NUM_FAULTS];
			objid_t reprid;
			char pad[256];
		} thread_kso_data;
		char _pad[4096];
	};
};

