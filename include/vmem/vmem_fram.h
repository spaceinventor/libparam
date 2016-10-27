/*
 * vmem_fram.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_FRAM_H_
#define SRC_PARAM_VMEM_FRAM_H_

#include <vmem/vmem.h>

typedef struct {
	int fram_addr;
} vmem_fram_driver_t;

#define VMEM_DEFINE_FRAM(name_in, strname, fram_addr_in, size_in, _vaddr) \
	static vmem_fram_driver_t vmem_##name_in##_driver = { \
		.fram_addr = fram_addr_in, \
	}; \
	static vmem_t vmem_##name_in##_instance __attribute__ ((section("vmem"), used)) = { \
		.name = strname, \
		.size = size_in, \
		.read = vmem_fram_read, \
		.write = vmem_fram_write, \
		.driver = &vmem_##name_in##_driver, \
		.vaddr = (void *) _vaddr, \
	}; \
	vmem_t * vmem_##name_in = &vmem_##name_in##_instance;

void vmem_fram_read(vmem_t * vmem, uint32_t addr, void * dataout, int len);
void vmem_fram_write(vmem_t * vmem, uint32_t addr, void * datain, int len);


#endif /* SRC_PARAM_VMEM_FRAM_H_ */
