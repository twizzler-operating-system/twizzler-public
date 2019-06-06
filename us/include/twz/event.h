#pragma once

#include <stdatomic.h>
#include <stdint.h>

struct evhdr {
	_Atomic uint64_t point;
};

struct event {
};
