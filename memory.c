
void *memset(void *ptr, int c, size_t len)
{
	char *p = ptr;
	while(len--) {
		*p++ = c;
	}
	return ptr;
}

