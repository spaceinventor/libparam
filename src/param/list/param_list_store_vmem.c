/*
 * param_list_store_vmem.c
 *
 *  Created on: Dec 19, 2016
 *      Author: johan
 */

#include <malloc.h>
#include <stdio.h>
#include <string.h>

#include <param/param_list.h>
#include "param_list.h"

#include <vmem/vmem.h>

#include <csp/csp.h>
#include <slash/slash.h>

void param_list_store_vmem_save(vmem_t * vmem) {
	char * hk_list = calloc(vmem->size, 1);

	FILE *stream = fmemopen(hk_list, vmem->size, "w");
	param_list_to_string(stream, -1, 1);
	fclose(stream);

	vmem_memcpy((void *) vmem->vaddr, hk_list, vmem->size);
	csp_hex_dump("hk list", hk_list, vmem->size);
	free(hk_list);
}

void param_list_store_vmem_load(vmem_t * vmem) {

	char * hk_list = calloc(vmem->size, 1);
	if (hk_list == NULL)
		return;

	//printf("Reading into %p %u bytes\n", hk_list, vmem->size);
	vmem_memcpy(hk_list, (void *) vmem->vaddr, vmem->size);
	//csp_hex_dump("hk list", hk_list, strlen(hk_list));

	FILE *stream = fmemopen(hk_list, strlen(hk_list), "r");
	param_list_from_string(stream, -1);
	fclose(stream);
	free(hk_list);

}
