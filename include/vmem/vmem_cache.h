#pragma once

#include <stdint.h>

struct vmem_cache_entry_s;
typedef struct vmem_cache_entry_s vmem_cache_entry_t;

typedef struct vmem_cache_entry_s {
    vmem_cache_entry_t *next;
    uint32_t address;
    uint8_t data[512];
} vmem_cache_entry_t;

typedef struct vmem_cache_s {
    vmem_cache_entry_t *cache;
} vmem_cache_t;

extern void cache_init(vmem_cache_t *me, vmem_cache_entry_t *entries, uint32_t nof_entries);
extern void cache_find_block(vmem_cache_t *me, uint32_t address, uint32_t length);