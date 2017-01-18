/*
 * param_group_slash.c
 *
 *  Created on: Jan 5, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <slash/slash.h>

#include <vmem/vmem.h>

#include <libparam.h>
#include <param/param_group.h>
#include "param_group.h"

static int groups(struct slash *slash)
{
	param_group_t * group;
	param_group_iterator i = {};
	while((group = param_group_iterate(&i)) != NULL) {
		printf("Group %s", group->name);
		if (group->interval > 0)
			printf(" interval: %u, %u:%u", (unsigned int) group->interval, group->node, group->port);
		printf("\n");

		for (int i = 0; i < group->count; i++) {
			param_print(group->params[i]);
		}
	}
	return SLASH_SUCCESS;
}
slash_command(groups, groups, NULL, NULL);

static int groups_str(struct slash *slash)
{
	param_group_to_string(stdout, "");
	return SLASH_SUCCESS;
}
slash_command_sub(groups, str, groups_str, NULL, NULL);

#if defined(PARAM_STORE_VMEM)

static int groups_save(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int id = atoi(slash->argv[1]);
	param_group_store_vmem_save(vmem_index_to_ptr(id));
	return SLASH_SUCCESS;
}
slash_command_sub(groups, save, groups_save, "<vmem_id>", NULL);

static int groups_load(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int id = atoi(slash->argv[1]);
	param_group_store_vmem_load(vmem_index_to_ptr(id));
	return SLASH_SUCCESS;
}
slash_command_sub(groups, load, groups_load, "<vmem_id>", NULL);

#elif defined(PARAM_STORE_FILE)

#endif
