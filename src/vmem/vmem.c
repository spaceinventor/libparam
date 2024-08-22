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

/**
 * @brief VMEM Memory copy function - 32-bit version ONLY
 * 
 * This method is only capable of handling 32-bit source and destination
 * addresses, which means that it CAN NOT handle the newest 64-bit addressing
 * 
 * @param to 32-bit destination address (virtual or physical)
 * @param from 32-bit source address (virtual or physical)
 * @param size Number fo bytes to copy
 * @return void* 
 */
void * vmem_memcpy(void * to, const void * from, uint32_t size) {

	return vmem_cpy((uint64_t)(uintptr_t)to, (uint64_t)(uintptr_t)from, (uint64_t)size);
}

/**
 * @brief Write chunk of data to VMEM from physical memory to virtual memory
 * 
 * @param to Virtual address to write the data to
 * @param from Physical address to read the data from
 * @param size Number of bytes to transfer 
 * @return void* 
 */
void * vmem_write(uint64_t to, const void * from, uint32_t size) {

	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {
		/* Write to VMEM */
		if ((to >= vmem->vaddr) && (to + (uint64_t)size <= vmem->vaddr + vmem->size)) {
			if (vmem->write) {
				vmem->write(vmem, to - vmem->vaddr, (void*)(uintptr_t)from, size);
			} else {
				memcpy((void *)(uintptr_t)to, (void *)(uintptr_t)from, size);
			}
			break;
		}
	}

	return NULL;
}

void * vmem_read(void * to, uint64_t from, uint32_t size) {

	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {
		/* Read */
		if ((from >= vmem->vaddr) && (from + (uint64_t)size <= vmem->vaddr + vmem->size)) {
			if (vmem->read) {
				vmem->read(vmem, from - vmem->vaddr, (void*)(uintptr_t)to, size);
			} else {
				memcpy((void *)(uintptr_t)to, (void *)(uintptr_t)from, size);
			}
			break;
		}
	}

	return NULL;
}

void * vmem_cpy(uint64_t to, uint64_t from, uint32_t size) {

	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {

		/* Write to VMEM */
		if ((to >= vmem->vaddr) && (to + (uint64_t)size <= vmem->vaddr + vmem->size)) {
			if (vmem->write) {
				vmem->write(vmem, to - vmem->vaddr, (void*)(uintptr_t)from, size);
				return NULL;
			}
		}

		/* Read */
		if ((from >= vmem->vaddr) && (from + (uint64_t)size <= vmem->vaddr + vmem->size)) {
			if (vmem->read) {
				vmem->read(vmem, from - vmem->vaddr, (void*)(uintptr_t)to, size);
				return NULL;
			}
		}

	}

	/* If no VMEM found or nor read/write methods exists for the particular VMEM */
	return memcpy((void*)(uintptr_t)to, (void*)(uintptr_t)from, size);

}

vmem_t * vmem_vaddr_to_vmem(uint64_t vaddr) {

	for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++) {

		/* Find VMEM from vaddr */
		if ((vaddr >= vmem->vaddr) && (vaddr <= vmem->vaddr + vmem->size)) {
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

