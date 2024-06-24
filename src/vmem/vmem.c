/*
 * vmem.c
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <csp/csp.h>

#include <vmem/vmem.h>

extern int __start_vmem, __stop_vmem;

/* The symbols __start_vmem and __stop_vmem will only be generated if the user defines any VMEMs.
    We therefore use __attribute__((weak)) so we can compile in the absence of these. */
extern __attribute__((weak)) int __start_vmem, __stop_vmem;

void * vmem_memcpy(void * to, const void * from, uint32_t size) {

	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {

		/* Write to VMEM */
		if ((to >= vmem->vaddr) && (to + size <= vmem->vaddr + vmem->size)) {
			//printf("Write to vmem %s, to %p from %p\n", vmem->name, to, from);
			vmem->write(vmem, to - vmem->vaddr, from, size);
			return NULL;
		}

		/* Read */
		if ((from >= vmem->vaddr) && (from + size <= vmem->vaddr + vmem->size)) {
			//printf("Read from vmem %s\n", vmem->name);
			vmem->read(vmem, from - vmem->vaddr, to, size);
			return NULL;
		}

	}

	/* If not vmem found */
	return memcpy(to, from, size);

}

vmem_t * vmem_vaddr_to_vmem(uint32_t vaddr) {

	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {

		/* Find VMEM from vaddr */
		if ((vaddr >= (uintptr_t)vmem->vaddr) && (vaddr <= (uintptr_t)vmem->vaddr + vmem->size)) {
			return vmem;
		}
	}

	return NULL;
}

int vmem_flush(vmem_t *vmem) {

	int res = 1;
	if (vmem && vmem->flush) {
		/* Call the flush method, which will empty any caches into the storage */
		res = vmem->flush(vmem);
	}
	return res;
}

vmem_t * vmem_index_to_ptr(int idx) {
	return ((vmem_t *) &__start_vmem) + idx;
}

int vmem_ptr_to_index(vmem_t * vmem) {
	return vmem - (vmem_t *) &__start_vmem;
}

