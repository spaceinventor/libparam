/*
 * vmem_slash.c
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#include <vmem/vmem.h>
#include <slash/slash.h>

slash_command_group(vmem, "VMEM");

static int slash_vmem_list(struct slash *slash)
{
	vmem_list();
	return SLASH_SUCCESS;
}
slash_command_sub(vmem, list, slash_vmem_list, NULL, "List vmems");
