#pragma once

#include <twz/_fault.h>
int twz_fault_set(int fault, void (*fn)(int, void *));
