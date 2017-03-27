/*
 * vmem_fram.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <stdint.h>
#include <string.h>

#include <vmem/vmem_fram.h>

#include <drivers/fm25w256.h>


void vmem_fram_read(const vmem_t * vmem, uint32_t addr, void * dataout, int len) {
	fm25w256_read_data(((intptr_t) ((vmem_fram_driver_t*) vmem->driver)->fram_addr) + addr, dataout, len);
}

void vmem_fram_write(const vmem_t * vmem, uint32_t addr, void * datain, int len) {
	fm25w256_write_data(((intptr_t) ((vmem_fram_driver_t*) vmem->driver)->fram_addr) + addr, datain, len);
}

