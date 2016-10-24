/*
 * vmem_fram_secure.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_FRAM_SECURE_H_
#define SRC_PARAM_VMEM_FRAM_SECURE_H_

#include <vmem/vmem.h>

void vmem_fram_secure_init(vmem_t * vmem);
void vmem_fram_secure_backup(vmem_t * vmem);
void vmem_fram_secure_read(vmem_t * vmem, uint16_t addr, void * dataout, int len);
void vmem_fram_secure_write(vmem_t * vmem, uint16_t addr, void * datain, int len);
void vmem_fram_secure_restore(vmem_t * vmem);

typedef const struct {
	uint8_t *data;
	int fram_primary_addr;
	int fram_backup_addr;
	void (*fallback_fct)(void);
} vmem_fram_secure_driver_t;

#define VMEM_DEFINE_FRAM_SECURE(name_in, strname, fram_primary_addr_in, fram_backup_addr_in, _fallback_fct, size_in, _vaddr) \
	uint8_t vmem_##name_in##_heap[size_in] = {}; \
	static vmem_fram_secure_driver_t vmem_##name_in##_driver = { \
		.data = vmem_##name_in##_heap, \
		.fram_primary_addr = fram_primary_addr_in, \
		.fram_backup_addr = fram_backup_addr_in, \
		.fallback_fct = _fallback_fct, \
	}; \
	vmem_t vmem_##name_in##_instance __attribute__ ((section("vmem"), used)) = { \
		.name = strname, \
		.size = size_in, \
		.read = vmem_fram_secure_read, \
		.write = vmem_fram_secure_write, \
		.driver = &vmem_##name_in##_driver, \
		.vaddr = (void *) _vaddr, \
	}; \
	vmem_t * vmem_##name_in = &vmem_##name_in##_instance;

#endif /* SRC_PARAM_VMEM_FRAM_SECURE_H_ */
