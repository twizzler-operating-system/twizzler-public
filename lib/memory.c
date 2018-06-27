
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

char *strnchr(char *s, int c, size_t n)
{
	while(n--) {
		char *t = s++;
		if(*t == c) return t;
		if(*t == 0) return NULL;
	}
	return NULL;
}

char *strncpy(char *d, const char *s, size_t n)
{
	char *_d = d;
	while(n-- && (*d++ = *s++))
		;
	return _d;
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

int strncmp(const char *s1, const char *s2, size_t n)
{
	while(n) {
		if(*s1 > *s2) return 1;
		else if(*s1 < *s2) return -1;
		else if(!*s1 && !*s2) return 0;
		s1++; s2++; n--;
	}
	return 0;
}

int strcmp(const char *s1, const char *s2)
{
	while(true) {
		if(*s1 > *s2) return 1;
		else if(*s1 < *s2) return -1;
		else if(!*s1 && !*s2) return 0;
		s1++; s2++;
	}
}

long strtol(char *str, char **end, int base)
{
	long tmp = 0;
	bool neg = false;
	if(*str == '-') {
		neg = true;
		str++;
	}
	if(*str == '+')
		str++;

	while(*str) {
		if(*str >= '0' && *str <= '0' + (base - 1)) {
			tmp *= base;
			tmp += *str - '0';
		} else if(*str >= 'a' && *str < 'a' + (base - 10)) {
			tmp *= base;
			tmp += *str - 'a';
		} else if(*str >= 'A' && *str < 'A' + (base - 10)) {
			tmp *= base;
			tmp += *str - 'A';
		} else {
			break;
		}

		str++;
	}

	if(end) *end = str;

	return neg ? -tmp : tmp;
}

