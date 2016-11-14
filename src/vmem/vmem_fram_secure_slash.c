/*
 * vmem_fram_secure_slash.c
 *
 *  Created on: Oct 5, 2016
 *      Author: johan
 */

#include <stdlib.h>

#include <vmem/vmem_fram_secure.h>

#include <slash/slash.h>

static int slash_vmem_backup(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int idx = atoi(slash->argv[1]);
	vmem_t * vmem = vmem_index_to_ptr(idx);
	vmem_fram_secure_backup(vmem);

	return SLASH_SUCCESS;
}
slash_command_sub(vmem, backup, slash_vmem_backup, "<vmem idx>", NULL);

static int slash_vmem_restore(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int idx = atoi(slash->argv[1]);
	vmem_t * vmem = vmem_index_to_ptr(idx);
	vmem_fram_secure_restore(vmem);

	return SLASH_SUCCESS;
}
slash_command_sub(vmem, restore, slash_vmem_restore, "<vmem idx>", NULL);


