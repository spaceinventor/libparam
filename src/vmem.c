/*
 * vmem.c
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */
#include <asf.h>
#include "vmem.h"

void vmem_dump(vmem_t * vmem)
{
	uint8_t data[vmem->size];
	vmem->read(vmem, 0x0, data, vmem->size);
	hex_dump(vmem->name, data, vmem->size);

}

void vmem_list(void) {
	extern int __start_vmem, __stop_vmem;
	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {
		printf(" [vmem_%u] (%u) %s\r\n", vmem - (vmem_t *) &__start_vmem, vmem->size, vmem->name);
	}
}

