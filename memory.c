
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

