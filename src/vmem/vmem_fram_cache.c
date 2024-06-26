/*
 * vmem_fram.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <stdint.h>
#include <string.h>

#include <vmem/vmem_fram_cache.h>

/**
 * Driver API declaration from:
 * <drivers/fram.h>
 */
void fram_write_data(uint32_t addr, const void *data, uint32_t len);
void fram_read_data(uint32_t addr, void *data, uint32_t len);


void vmem_fram_cache_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len) {

	vmem_fram_cache_driver_t * driver = vmem->driver;

	if (driver->cache_status == VMEM_FRAM_CACHE_EMPTY) {
		fram_read_data(driver->fram_addr, driver->cache, vmem->size);
		//printf("Fram addr %x, driver cache %p\n", driver->fram_addr, driver->cache);
		//csp_hex_dump("init cache", driver->cache, vmem->size);
		driver->cache_status = VMEM_FRAM_CACHE_INITED;
	}

	//printf("Cached read %s addr %lu len %d\n", vmem->name, addr, len);
	memcpy(dataout, driver->cache + addr, len);

}

void vmem_fram_cache_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len) {

	vmem_fram_cache_driver_t * driver = vmem->driver;

	//printf("Cached write %s addr %lu len %d\n", vmem->name, addr, len);

	memcpy(driver->cache + addr, datain, len);
	fram_write_data(driver->fram_addr + addr, datain, len);

}

