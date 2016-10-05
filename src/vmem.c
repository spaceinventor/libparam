/*
 * vmem.c
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */
#include <asf.h>
#include "vmem.h"

extern int __start_vmem, __stop_vmem;

void vmem_dump(vmem_t * vmem)
{
	uint8_t data[vmem->size];
	vmem->read(vmem, 0x0, data, vmem->size);
	hex_dump(vmem->name, data, vmem->size);

}

void vmem_list(void) {
	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {
		printf(" [vmem_%u] (%u) %s at %p\r\n", vmem - (vmem_t *) &__start_vmem, vmem->size, vmem->name, vmem->vaddr);
	}
}

void * vmem_memcpy(void * to, void * from, size_t size) {

	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {

		/* Write to VMEM */
		if ((to >= vmem->vaddr) && (to + size <= vmem->vaddr + vmem->size)) {
			printf("Write to vmem %s\n", vmem->name);
			vmem->write(vmem, to - vmem->vaddr, from, size);
			return NULL;
		}

		/* Read */
		if ((from >= vmem->vaddr) && (from + size <= vmem->vaddr + vmem->size)) {
			printf("Read from vmem %s\n", vmem->name);
			vmem->read(vmem, from - vmem->vaddr, to, size);
			return NULL;
		}

	}

	/* If not vmem found */
	return memcpy(to, from, size);

}
