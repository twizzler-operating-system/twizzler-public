#pragma once

#include <twz/_fault.h>
int twz_fault_set(int fault, void (*fn)(int, void *, void *), void *);
void *twz_fault_get_userdata(int fault);
void twz_fault_raise_data(int fault, void *data, void *userdata);
void twz_fault_raise(int fault, void *data);
