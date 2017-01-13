/*
 * param_group_store_vmem.c
 *
 *  Created on: Jan 13, 2017
 *      Author: johan
 */

#include <csp/csp.h>

#include <vmem/vmem.h>

#include <param/param.h>
#include <param/param_group.h>
#include "param_group.h"

void param_group_store_vmem_save(vmem_t * vmem) {
	char * buf = calloc(vmem->size, 1);
	if (buf == NULL)
		return;

	FILE *stream = fmemopen(buf, vmem->size, "w");
	param_group_to_string(stream, "");
	fclose(stream);

	vmem_memcpy((void *) vmem->vaddr, buf, vmem->size);
	csp_hex_dump("hk grp", buf, vmem->size);
	free(buf);
}

void param_group_store_vmem_load(vmem_t * vmem) {

	char * buf = calloc(vmem->size, 1);
	if (buf == NULL)
		return;

	vmem_memcpy(buf, (void *) vmem->vaddr, vmem->size);
	csp_hex_dump("hk grp", buf, strlen(buf));

	FILE *stream = fmemopen(buf, strlen(buf), "r");
	param_group_from_string(stream);
	fclose(stream);
	free(buf);

}
