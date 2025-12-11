/*
 * vmem.c
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <csp/csp.h>

#include <vmem/vmem.h>

void * vmem_memcpy(void * to, const void * from, uint32_t size) {

	return vmem_cpy((uint64_t)(uintptr_t)to, (uint64_t)(uintptr_t)from, (uint64_t)size);
}

void * vmem_write_direct(vmem_t * vmem, uint64_t to, const void * from, uint32_t size) {

	/* Write to VMEM */
	if ((to >= vmem->vaddr) && (to + (uint64_t)size <= vmem->vaddr + vmem->size)) {
		if (vmem->write) {
			vmem->write(vmem, to - vmem->vaddr, (void*)(uintptr_t)from, size);
		} else {
			memcpy((void *)(uintptr_t)to, (void *)(uintptr_t)from, size);
		}
	}

	return NULL;
}

void * vmem_read_direct(vmem_t * vmem, void * to, uint64_t from, uint32_t size) {

	/* Read */
	if ((from >= vmem->vaddr) && (from + (uint64_t)size <= vmem->vaddr + vmem->size)) {
		if (vmem->read) {
			vmem->read(vmem, from - vmem->vaddr, (void*)(uintptr_t)to, size);
		} else {
			memcpy((void *)(uintptr_t)to, (void *)(uintptr_t)from, size);
		}
	}

	return NULL;
}

void * vmem_write(uint64_t to, const void * from, uint32_t size) {
	vmem_t *vmem;
	vmem_iter_t start = {.idx = -1};
	for (vmem_iter_t *iter = vmem_next(&start); iter != NULL; iter = vmem_next(iter)) {
		vmem = vmem_from_iter(iter);
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
	vmem_t *vmem;
	vmem_iter_t start = {.idx = -1};
	for (vmem_iter_t *iter = vmem_next(&start); iter != NULL; iter = vmem_next(iter)) {
		vmem = vmem_from_iter(iter);
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
	vmem_t *vmem;
	vmem_iter_t start = {.idx = -1};
	for (vmem_iter_t *iter = vmem_next(&start); iter != NULL; iter = vmem_next(iter)) {
		vmem = vmem_from_iter(iter);

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

	vmem_t *vmem;
	vmem_iter_t start = {.idx = -1};
	for (vmem_iter_t *iter = vmem_next(&start); iter != NULL; iter = vmem_next(iter)) {
		vmem = vmem_from_iter(iter);

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
	vmem_iter_t start = {.idx = -1};
	for (vmem_iter_t *iter = vmem_next(&start); iter != NULL; iter = vmem_next(iter)) {
		if(iter->idx == idx) {
			return iter->current;		
		}
	}
	return NULL;
}

int vmem_ptr_to_index(vmem_t * vmem) {
	vmem_iter_t start = {.idx = -1};
	for (vmem_iter_t *iter = vmem_next(&start); iter != NULL; iter = vmem_next(iter)) {
		if(iter->current == vmem) {
			return iter->idx;		
		}
	}
	return -1;
}

static vmem_iter_t g_start = {
	.start = (vmem_t *) &__start_vmem,
	.stop = (vmem_t *) &__stop_vmem,
	.current = NULL,
	.idx = 0,
	.next = NULL
};

vmem_iter_t *vmem_next(vmem_iter_t * iter) {
	if(iter->idx == -1) {
		iter->idx = 0;
		iter->current = g_start.start;
		iter->start = g_start.start;
		iter->stop = g_start.stop;
		if (g_start.start == NULL) {
			return NULL;
		};
	} else {
		if(iter->current < (iter->stop - 1)) {
			iter->current++;
			iter->idx++;
		} else {
			if(iter->next) {
				iter->current = iter->start;
				iter->next->idx = iter->idx + 1;
				iter = iter->next;
				iter->current = iter->start;
			} else {
				iter = NULL;
			}
		}
	}
	return iter;
}

vmem_t *vmem_from_iter(vmem_iter_t * iter) {
	vmem_t *res = NULL;
	if(iter) {
		res = iter->current;
	}
	return res;
}

void vmem_add(vmem_t * start, vmem_t * stop) {
	/* Handle case when host application has no VMEM (its __start_vmem/__stop_vmem are NULL) */
	if(NULL == g_start.start) {
		g_start.start = start;
		g_start.stop = stop;
		g_start.current = start;
		g_start.idx = 0;
		g_start.next = NULL;
	} else {
		/* 
		* APMs without VMEMs will inherit the __start_vmem/__stop_vmem symbols from the 
		* host application (CSH) so we check for those being already in our list
		* of VMEM blocks
		*/
		if(start != g_start.start) {
			vmem_iter_t *new_vmem = calloc(sizeof(vmem_iter_t), 1);
			if(new_vmem) {
				new_vmem->start = start;
				new_vmem->stop = stop;
				new_vmem->current = start;
				vmem_iter_t start = {.idx = -1};
				for (vmem_iter_t *iter = vmem_next(&start); iter != NULL; iter = vmem_next(iter)) {
					if(!iter->next) {
						iter->next = new_vmem;
						break;
					}
				}
			}
		}
	}
}
