
void *memset(void *ptr, int c, size_t len)
{
	char *p = ptr;
	while(len--) {
		*p++ = c;
	}
	return ptr;
}

size_t strlen(const char *s)
{
	size_t c = 0;
	while(*s++) c++;
	return c;
}

void *memcpy(void *dest, const void *src, size_t len)
{
	char *d = dest;
	const char *s = src;
	while(len--) {
		*d++ = *s++;
	}
	return dest;
}

int memcmp(const void* ptr1, const void* ptr2, size_t num) {
    const unsigned char* vptr1 = (const unsigned char*)ptr1;
    const unsigned char* vptr2 = (const unsigned char*)ptr2;
    while (num) {
        if (*vptr1 > *vptr2) return 1;
        else if (*vptr1 < *vptr2) return -1;
        vptr1++; vptr2++; num--;
    }
    return 0;
}


