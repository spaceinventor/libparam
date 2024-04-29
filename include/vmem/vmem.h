/*
 * vmem.h
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_H_
#define SRC_PARAM_VMEM_H_

#define VMEM_MAX(a,b) ((a) > (b) ? a : b)
#define VMEM_MIN(a,b) ((a) < (b) ? a : b)

#include <stddef.h>
#include <param/param.h>

enum vmem_types{
	VMEM_TYPE_RAM = 1,
	VMEM_TYPE_FRAM = 2,
	VMEM_TYPE_FRAM_SECURE = 3,
	VMEM_TYPE_FLASH = 4,
	VMEM_TYPE_DRIVER = 5,
	VMEM_TYPE_FLASH_QSPI = 6,
	VMEM_TYPE_FILE = 7,
	VMEM_TYPE_FRAM_CACHE = 8,
	VMEM_TYPE_NOR_FLASH = 9,
};

typedef struct vmem_s {
	int type;
	void (*read)(struct vmem_s * vmem, uint64_t addr, void * dataout, intptr_t len);
	void (*write)(struct vmem_s * vmem, uint64_t addr, const void * datain, intptr_t len);
	int (*backup)(struct vmem_s * vmem);
	int (*restore)(struct vmem_s * vmem);
	uint64_t vaddr;
	intptr_t size;
	const char *name;
	int big_endian;
	int ack_with_pull; // allow ack with pull request
	void * driver;
} vmem_t;

void * vmem_memcpy(uint64_t to, uint64_t from, intptr_t size);
vmem_t * vmem_index_to_ptr(int idx);
int vmem_ptr_to_index(vmem_t * vmem);

extern int __start_vmem, __stop_vmem;

#endif /* SRC_PARAM_VMEM_H_ */
