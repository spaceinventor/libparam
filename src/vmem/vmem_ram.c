/*
 * vmem_ram.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <stdint.h>
#include <string.h>

#include <vmem/vmem_ram.h>

void vmem_ram_read(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t len) {
	memcpy(dataout, ((vmem_ram_driver_t *) vmem->driver)->physaddr + addr, len);
}

void vmem_ram_write(vmem_t * vmem, uint32_t addr, const void * datain, uint32_t len) {
	memcpy(((vmem_ram_driver_t *) vmem->driver)->physaddr + addr, datain, len);
}
