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
	uint32_t fram_addr;
} vmem_fram_driver_t;

#define VMEM_DEFINE_FRAM(name_in, strname, fram_addr_in, size_in, _vaddr) \
	static vmem_fram_driver_t vmem_##name_in##_driver = { \
		.fram_addr = fram_addr_in, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_FRAM, \
		.name = strname, \
		.size = size_in, \
		.read = vmem_fram_read, \
		.write = vmem_fram_write, \
		.driver = &vmem_##name_in##_driver, \
		.vaddr = _vaddr, \
		.ack_with_pull = 1, \
	};

void vmem_fram_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len);
void vmem_fram_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len);


#endif /* SRC_PARAM_VMEM_FRAM_H_ */
