/*
 * vmem_file.h
 *
 *  Created on: Aug 30, 2018
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_FILE_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_FILE_H_

#include <vmem/vmem.h>

typedef struct {
	void * physaddr;
	char * filename;
} vmem_file_driver_t;

void vmem_file_init(vmem_t * vmem);
void vmem_file_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len);
void vmem_file_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len);

#define VMEM_DEFINE_FILE(name_in, strname, filename_in, size_in) \
	uint8_t vmem_##name_in##_buf[size_in] = {}; \
	static vmem_file_driver_t vmem_##name_in##_driver = { \
		.physaddr = vmem_##name_in##_buf, \
		.filename = filename_in, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	vmem_t vmem_##name_in = { \
		.type = VMEM_TYPE_FILE, \
		.name = strname, \
		.size = size_in, \
		.read = vmem_file_read, \
		.write = vmem_file_write, \
		.driver = &vmem_##name_in##_driver, \
		.vaddr = (uint64_t)vmem_##name_in##_buf, \
		.ack_with_pull = 1, \
	};


#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_FILE_H_ */
