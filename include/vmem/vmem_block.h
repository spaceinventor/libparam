/*
 * vmem_block.h
 *
 *  Created on: May 28, 2024
 *      Author: thomas lykkeberg
 */

#ifndef VMEM_BLOCK_H_
#define VMEM_BLOCK_H_

#include <stdint.h>
#include <stdbool.h>
#include <vmem/vmem.h>

/* Forward definitions of driver and device objects */
struct vmem_block_driver_s;
typedef struct vmem_block_driver_s vmem_block_driver_t;
struct vmem_block_device_s;
typedef struct vmem_block_device_s vmem_block_device_t;

typedef struct vmem_block_cache_s {
    bool is_valid;
    bool is_modified;
    uint32_t start_block;
    uint32_t size;
    uint8_t *data;
} vmem_block_cache_t;

/* Block API methods */
typedef int32_t vmem_bwrite_t(const vmem_block_driver_t *drv, uint32_t blockaddr, uint32_t n_blocks, uint8_t *data);
typedef int32_t vmem_bread_t(const vmem_block_driver_t *drv, uint32_t blockaddr, uint32_t n_blocks, uint8_t *data);
typedef int32_t vmem_binit_t(const vmem_block_device_t *dev);

typedef struct vmem_block_driver_api_s {
	vmem_bwrite_t * const write;
	vmem_bread_t * const read;
} vmem_block_driver_api_t;

typedef struct vmem_block_device_s {
	char *name;
	uint32_t bsize;
	uint32_t total_nblocks;
	vmem_block_cache_t *cache;
	vmem_binit_t * const init;
} vmem_block_device_t;

typedef struct vmem_block_driver_s {
	char *name;
	vmem_block_device_t * const device;
	const vmem_block_driver_api_t api;
} vmem_block_driver_t;

typedef struct vmem_block_region_s {
	uint64_t physaddr;
	vmem_block_driver_t * const driver;
} vmem_block_region_t;

#define VMEM_DEFINE_BLOCK_CACHE(name_in, _csize) \
	static uint8_t vmem_##name_in##_cache_data[_csize]; \
	static const vmem_block_cache_t vmem_##name_in##_cache = { \
		.is_valid = false, \
		.is_modified = false, \
		.start_block = 0, \
		.size = _csize, \
		.data = &vmem_##name_in##_cache_data[0], \
	};

/* Macros for defining block- devices, drivers and regions */
#define VMEM_DEFINE_BLOCK_DEVICE(name_in, strname, _bsize, _total_nblocks, init_fn, cache_name) \
	__attribute__((section("vmem_bdevice"))) \
	__attribute__((aligned(4))) \
	__attribute__((used)) \
	static const vmem_block_device_t vmem_##name_in##_device = { \
		.name = strname, \
		.bsize = (_bsize), \
		.total_nblocks = (_total_nblocks), \
		.cache = &vmem_##cache_name##_cache, \
		.init = init_fn, \
	};

#define VMEM_DEFINE_BLOCK_DRIVER(name_in, strname, read_fn, write_fn, device_in) \
	static const vmem_block_driver_t vmem_##name_in##_driver = { \
		.name = strname, \
		.api = { \
			.read = read_fn, \
			.write = write_fn, \
		}, \
		.device = &vmem_##device_in##_device, \
	}; \

#define VMEM_DEFINE_BLOCK_REGION(name_in, strname, addr_in, size_in, _vaddr, driver_in) \
	static const vmem_block_region_t vmem_##name_in##_region = { \
		.physaddr = addr_in, \
		.driver = &vmem_##driver_in##_driver, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(4))) \
	__attribute__((used)) \
	vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_BLOCK, \
		.name = strname, \
		.size = (size_in), \
		.read = vmem_block_read, \
		.write = vmem_block_write, \
		.driver = &vmem_##name_in##_region, \
		.vaddr = (void *) _vaddr, \
	};

extern void vmem_block_read(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t len);
extern void vmem_block_write(vmem_t * vmem, uint32_t addr, const void * datain, uint32_t len);
extern void vmem_block_init(void);

#endif /* VMEM_BLOCK_H_ */
