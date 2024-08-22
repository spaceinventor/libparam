/*
 * vmem_ram.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_RAM_H_
#define SRC_PARAM_VMEM_RAM_H_

#include <vmem/vmem.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void * physaddr;
} vmem_ram_driver_t;

#include <stdint.h>
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFULL
#define __64BIT__ 1
#else
#define __64BIT__ 0
#endif

#if __64BIT__
#define VMEM_STATIC_RAM_VADDR_INITIALIZER(name_in) \
		.vaddr = (uint64_t)&vmem_##name_in##_heap
#else
#define VMEM_STATIC_RAM_VADDR_INITIALIZER(name_in) \
		.vaddr32 = (uint32_t)&vmem_##name_in##_heap, \
		.vaddr_pad = 0
#endif

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
		.read = NULL, \
		.write = NULL, \
		VMEM_STATIC_RAM_VADDR_INITIALIZER(name_in), \
		.size = size_in, \
		.name = strname, \
		.ack_with_pull = 1, \
		.driver = &vmem_##name_in##_driver, \
	};

#if __64BIT__
#define VMEM_STATIC_RAM_ADDR_VADDR_INITIALIZER(mem_addr) \
		.vaddr = (uint64_t)mem_addr
#else
#define VMEM_STATIC_RAM_ADDR_VADDR_INITIALIZER(mem_addr) \
		.vaddr32 = (uint32_t)mem_addr, \
		.vaddr_pad = 0
#endif

#define VMEM_DEFINE_STATIC_RAM_ADDR(name_in, strname, size_in, mem_addr) \
    static vmem_ram_driver_t vmem_##name_in##_driver = { \
        .physaddr = mem_addr, \
    }; \
    __attribute__((section("vmem"))) \
    __attribute__((aligned(1))) \
    __attribute__((used)) \
    vmem_t vmem_##name_in = { \
        .type = VMEM_TYPE_RAM, \
        .read = NULL, \
        .write = NULL, \
		VMEM_STATIC_RAM_ADDR_VADDR_INITIALIZER(mem_addr), \
        .size = size_in, \
        .name = strname, \
        .ack_with_pull = 1, \
        .driver = &vmem_##name_in##_driver, \
    };

#ifdef __cplusplus
}
#endif

#endif /* SRC_PARAM_VMEM_RAM_H_ */
