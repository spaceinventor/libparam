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

VMEM_SECTION_INIT_NO_FUNC(vmem)

static bool vmem_list_initialized = false;
static vmem_t * vmem_head = 0;

void * vmem_memcpy(void * to, void * from, size_t size) {

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

vmem_t * vmem_index_to_ptr(int idx) {

    vmem_t * vmem = vmem_list_head();
    while (idx && vmem) {
        vmem = vmem_list_iterate(vmem);
        idx--;
    }
	return vmem;
}

int vmem_ptr_to_index(vmem_t * vmem) {

    int idx = 0;
    for (vmem_t * v = vmem_list_head(); v; v = vmem_list_iterate(v)) {
        if (v == vmem) {
            return idx;
        }
        idx++;
    }
	return -1;
}

vmem_t * vmem_list_insert(vmem_t * head, vmem_t * vmem) {

    /* vmem->next: first entry  prev: none */
    vmem->next = head;
    vmem_t * prev = 0;

    /* As long af vmem->next is alphabetically lower (buble-sort)) */
    while (vmem->next && (strcmp(vmem->name, vmem->next->name) > 0)) {
        /* vmem->next: next entry  prev: previous entry */
        prev = vmem->next;
        vmem->next = vmem->next->next;
    }

    if (prev) {
        /* Insert before vmem->next */
        prev->next = vmem;
    } else {
        /* Insert as first entry */
        head = vmem;
    }

    return head;
}

vmem_t * vmem_list_add_section(vmem_t * head, vmem_t * start, vmem_t *stop)
{

	for (vmem_t * vmem = start; vmem < stop; ++vmem) {
        head = vmem_list_insert(head, vmem);
	}

    return head;
}

vmem_t * vmem_list_head() {

    if (!vmem_list_initialized) {
        vmem_list_initialized = true;
        vmem_head = vmem_list_add_section(0, vmem_section_start, vmem_section_stop);
    }

    return vmem_head;
}

vmem_t * vmem_list_iterate(vmem_t * vmem) {

    return vmem ? vmem->next : 0;
}
