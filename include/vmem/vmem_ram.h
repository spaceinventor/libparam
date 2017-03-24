/*
 * vmem_ram.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_RAM_H_
#define SRC_PARAM_VMEM_RAM_H_

#include <vmem/vmem.h>

void vmem_ram_read(const vmem_t * vmem, uint32_t addr, void * dataout, int len);
void vmem_ram_write(const vmem_t * vmem, uint32_t addr, void * datain, int len);

typedef struct {
	void * physaddr;
} vmem_ram_driver_t;

#define VMEM_DEFINE_STATIC_RAM(name_in, strname, size_in) \
	uint8_t vmem_##name_in##_heap[size_in] = {}; \
	static const vmem_ram_driver_t vmem_##name_in##_driver = { \
		.physaddr = vmem_##name_in##_heap, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	const vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_RAM, \
		.name = strname, \
		.size = size_in, \
		.read = vmem_ram_read, \
		.write = vmem_ram_write, \
		.driver = &vmem_##name_in##_driver, \
		.vaddr = vmem_##name_in##_heap, \
	};

#endif /* SRC_PARAM_VMEM_RAM_H_ */
