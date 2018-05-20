#pragma once

static inline void bitmap_assign(void *ptr, int bit, int val)
{
    int index = bit / 8;
    int offset = bit % 8;
    uint8_t *tmp = ptr;
    if(val)
        tmp[index] |= (1 << offset);
    else
        tmp[index] &= ~(1 << offset);
}

static inline void bitmap_set(void *ptr, int bit)
{
	uint8_t *tmp = ptr;
	tmp[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_reset(void *ptr, int bit)
{
	uint8_t *tmp = ptr;
	tmp[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(void *ptr, int bit)
{
    uint8_t *tmp = ptr;
    return (tmp[bit / 8] & (1 << (bit % 8)));
}

static inline int bitmap_ffs(void *ptr, int numbits)
{
    uint8_t *tmp = ptr;
    for(int i=0;i<numbits;i++) {
        int index = i / 8;
        int offset = i % 8;
        if(tmp[index] & (1 << offset))
            return i;
    }
    return -1;
}

static inline int bitmap_ffr(void *ptr, int numbits)
{
    uint8_t *tmp = ptr;
    for(int i=0;i<numbits;i++) {
        int index = i / 8;
        int offset = i % 8;
        if(!(tmp[index] & (1 << offset)))
            return i;
    }
    return -1;
}

static inline int bitmap_ffr_start(void *ptr, int numbits, int start)
{
    uint8_t *tmp = ptr;
    for(int i=start;i<numbits;i++) {
        int index = i / 8;
        int offset = i % 8;
        if(!(tmp[index] & (1 << offset)))
            return i;
    }
    return -1;
}

