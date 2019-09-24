#pragma once

void *krealloc(void *p, size_t sz);
void *krecalloc(void *p, size_t num, size_t sz);
void *kalloc(size_t sz);
void *kcalloc(size_t num, size_t sz);
void kfree(void *p);
