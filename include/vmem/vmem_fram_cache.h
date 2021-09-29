/*
 * vmem_fram.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#pragma once

#include <vmem/vmem.h>

enum {
	VMEM_FRAM_CACHE_EMPTY,
	VMEM_FRAM_CACHE_INITED,
};

typedef struct {
	int fram_addr;
	uint8_t *cache;
	int cache_status;
} vmem_fram_cache_driver_t;

#define VMEM_DEFINE_FRAM_CACHE(name_in, strname, fram_addr_in, size_in, _vaddr) \
    uint8_t vmem_##name_in##_cache[size_in] __attribute__((section(".noinit"))); \
	static vmem_fram_cache_driver_t vmem_##name_in##_driver = { \
		.fram_addr = fram_addr_in, \
		.cache = vmem_##name_in##_cache, \
		.cache_status = 0, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	const vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_FRAM_CACHE, \
		.name = strname, \
		.size = size_in, \
		.read = vmem_fram_cache_read, \
		.write = vmem_fram_cache_write, \
		.driver = &vmem_##name_in##_driver, \
		.vaddr = (void *) _vaddr, \
	};

void vmem_fram_cache_read(const vmem_t * vmem, uint32_t addr, void * dataout, int len);
void vmem_fram_cache_write(const vmem_t * vmem, uint32_t addr, void * datain, int len);
