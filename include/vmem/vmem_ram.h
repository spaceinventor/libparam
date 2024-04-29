/*
 * vmem_ram.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_RAM_H_
#define SRC_PARAM_VMEM_RAM_H_

#include <vmem/vmem.h>

void vmem_ram_read(vmem_t * vmem, uint64_t addr, void * dataout, intptr_t len);
void vmem_ram_write(vmem_t * vmem, uint64_t addr, const void * datain, intptr_t len);

typedef struct {
	void * physaddr;
} vmem_ram_driver_t;

#define VMEM_DEFINE_STATIC_RAM(name_in, strname, size_in) \
	uint8_t vmem_##name_in##_heap[size_in] = {}; \
	static vmem_ram_driver_t vmem_##name_in##_driver = { \
		.physaddr = vmem_##name_in##_heap, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_RAM, \
		.name = strname, \
		.size = size_in, \
		.read = vmem_ram_read, \
		.write = vmem_ram_write, \
		.driver = &vmem_##name_in##_driver, \
		.vaddr = vmem_##name_in##_heap, \
		.ack_with_pull = 1, \
	};

#define VMEM_DEFINE_STATIC_RAM_ADDR(name_in, strname, size_in, mem_addr) \
    static vmem_ram_driver_t vmem_##name_in##_driver = { \
        .physaddr = mem_addr, \
    }; \
    __attribute__((section("vmem"))) \
    __attribute__((aligned(1))) \
    __attribute__((used)) \
    vmem_t vmem_##name_in = { \
        .type = VMEM_TYPE_RAM, \
        .name = strname, \
        .size = size_in, \
        .read = vmem_ram_read, \
        .write = vmem_ram_write, \
        .driver = &vmem_##name_in##_driver, \
        .vaddr = 0, \
        .ack_with_pull = 1, \
    };

#endif /* SRC_PARAM_VMEM_RAM_H_ */
