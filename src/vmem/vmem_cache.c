#include <stddef.h>

#include "vmem/vmem_cache.h"

void cache_init(vmem_cache_t *me, vmem_cache_entry_t *entries, uint32_t nof_entries) {

    me->cache = NULL;
    while (nof_entries > 0) {
        entries->next = me->cache;
        me->cache = entries;
        nof_entries--;
    }

}

void cache_find_block(vmem_cache_t *me, uint32_t address, uint32_t length) {

}