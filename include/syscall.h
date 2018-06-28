#pragma once

#include <arch/syscall.h>

#define NUM_SYSCALLS 8
#define NUM_SYSCALLS128 1

long syscall_thread_spawn(__int128 foo);

