/*
 * vmem_fram.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <stdint.h>
#include <string.h>

#include <vmem/vmem_fram.h>

/**
 * Driver API declaration from:
 * <drivers/fram.h>
 */
void fram_write_data(uint32_t addr, const void *data, uint32_t len);
void fram_read_data(uint32_t addr, void *data, uint32_t len);


void vmem_fram_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len) {
	fram_read_data(((uintptr_t) ((vmem_fram_driver_t*) vmem->driver)->fram_addr) + (uintptr_t)addr, dataout, len);
}

void vmem_fram_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len) {
	fram_write_data(((uintptr_t) ((vmem_fram_driver_t*) vmem->driver)->fram_addr) + (uintptr_t)addr, datain, len);
}

