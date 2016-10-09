/*
 * vmem_slash.c
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#include <vmem/vmem.h>
#include <slash/slash.h>

static int slash_vmem_list(struct slash *slash)
{
	vmem_list();
	return SLASH_SUCCESS;
}
slash_command(vmem_list, slash_vmem_list, NULL, "List vmems");
