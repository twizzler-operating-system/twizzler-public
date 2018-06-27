#pragma once

void *memset(void *ptr, int c, size_t len);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t len);
int memcmp(const void* ptr1, const void* ptr2, size_t num);
char *strnchr(char *s, int c, size_t n);
char *strncpy(char *d, const char *s, size_t n);
int strcmp(const char *s1, const char *s2);

