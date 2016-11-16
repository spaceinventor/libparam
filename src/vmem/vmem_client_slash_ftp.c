/*
 * vmem_client_slash.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <stdio.h>
#include <sys/stat.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <vmem/vmem_client.h>

#include <slash/slash.h>

static int vmem_client_slash_list(struct slash *slash)
{
	int node = csp_get_address();
	int timeout = 2000;
	char * endptr;

	if (slash->argc >= 2) {
		node = strtoul(slash->argv[1], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	if (slash->argc >= 3) {
		timeout = strtoul(slash->argv[2], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	printf("Requesting vmem list from node %u timeout %u\n", node, timeout);

	vmem_client_list(node, timeout);
	return SLASH_SUCCESS;
}
slash_command_sub(vmem, list, vmem_client_slash_list, "[node] [timeout]", NULL);

static int vmem_client_slash_fram(struct slash *slash, int backup) {

	int node = csp_get_address();
	int vmem_id;
	int timeout = 2000;
	char * endptr;

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	vmem_id = strtoul(slash->argv[1], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	if (slash->argc >= 3) {
		node = strtoul(slash->argv[2], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	if (slash->argc >= 4) {
		timeout = strtoul(slash->argv[3], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	if (backup) {
		printf("Taking backup of vmem %u on node %u\n", vmem_id, node);
	} else {
		printf("Restoring vmem %u on node %u\n", vmem_id, node);
	}

	int result = vmem_client_backup(node, vmem_id, timeout, backup);
	if (result == -2) {
		printf("No response\n");
	} else {
		printf("Result: %d\n", result);
	}

	return SLASH_SUCCESS;
}

static int vmem_client_slash_restore(struct slash *slash)
{
	return vmem_client_slash_fram(slash, 0);
}
slash_command_sub(vmem, restore, vmem_client_slash_restore, "<vmem idx> [node] [timeout]", NULL);

static int vmem_client_slash_backup(struct slash *slash)
{
	return vmem_client_slash_fram(slash, 1);
}
slash_command_sub(vmem, backup, vmem_client_slash_backup, "<vmem idx> [node] [timeout]", NULL);


