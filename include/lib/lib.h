#pragma once

void qsort(void* base, size_t num, size_t size, int (*compar)(const void*,const void*));

/* from wikipedia */
static inline
uint32_t isqrt(uint32_t op)
{
    uint32_t res = 0;
    uint32_t one = 1uL << 30;
    while (one > op) {
        one >>= 2;
    }
    while (one != 0) {
        if (op >= res + one) {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }
    return res;
}

