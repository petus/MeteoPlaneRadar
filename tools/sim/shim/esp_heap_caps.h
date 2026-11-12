#pragma once
#include <stdlib.h>
#include <stdint.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static inline void* heap_caps_malloc(size_t n, uint32_t) { return malloc(n); }
static inline void* heap_caps_calloc(size_t n, size_t sz, uint32_t) { return calloc(n, sz); }
static inline void heap_caps_free(void* p) { free(p); }
