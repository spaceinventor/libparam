#pragma once

#include <vmem/vmem.h>

void * vmem_write_direct(vmem_t * vmem, uint64_t to, const void * from, uint32_t size);
void * vmem_read_direct(vmem_t * vmem, void * to, uint64_t from, uint32_t size);
