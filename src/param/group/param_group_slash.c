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

static int group(struct slash *slash)
{
	param_group_t * group;
	param_group_iterator i = {};
	while((group = param_group_iterate(&i)) != NULL) {

		if ((slash->argc >= 2) && (strcmp(slash->argv[1], group->name) != 0))
			continue;

		printf("Group %s", group->name);
		if (group->interval > 0)
			printf(" interval: %u, %u", (unsigned int) group->interval, group->node);
		printf("\n");

		for (int i = 0; i < group->count; i++) {
			param_print(group->params[i], NULL, 0, 2);
		}
	}
	return SLASH_SUCCESS;
}
slash_command(group, group, "[group_name]", NULL);

static int group_str(struct slash *slash)
{
	param_group_to_string(stdout, "");
	return SLASH_SUCCESS;
}
slash_command_sub(group, str, group_str, NULL, NULL);

#if defined(PARAM_STORE_VMEM)

static int group_save(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int id = atoi(slash->argv[1]);
	param_group_store_vmem_save(vmem_index_to_ptr(id));
	return SLASH_SUCCESS;
}
slash_command_sub(group, save, group_save, "<vmem_id>", NULL);

static int group_load(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	int id = atoi(slash->argv[1]);
	param_group_store_vmem_load(vmem_index_to_ptr(id));
	return SLASH_SUCCESS;
}
slash_command_sub(group, load, group_load, "<vmem_id>", NULL);

#elif defined(PARAM_STORE_FILE)

static int param_group_store_file_save_slash(struct slash *slash)
{
	param_group_store_file_save("group.cfg");
	return SLASH_SUCCESS;
}
slash_command_sub(group, save, param_group_store_file_save_slash, NULL, NULL);

static int param_group_store_file_load_slash(struct slash *slash)
{
	param_group_store_file_load("group.cfg");
	return SLASH_SUCCESS;
}
slash_command_sub(group, load, param_group_store_file_load_slash, NULL, NULL);

#endif

static int group_pull_slash(struct slash *slash)
{
	if (slash->argc < 2)
		return SLASH_EUSAGE;

	param_group_t * group = param_group_find_name(slash->argv[1]);
	if (group == NULL)
		return SLASH_EINVAL;

	unsigned int timeout = 1000;

	int hostarr[10];
	int hostarr_cnt = 0;
	if (slash->argc >= 3) {

		char * saveptr = NULL;
		char * token;
		token = strtok_r(slash->argv[2], ",", &saveptr);
		while(token != NULL) {
			sscanf(token, "%d", &hostarr[hostarr_cnt++]);
			token = strtok_r(NULL, ",", &saveptr);
		};

	}
	if (slash->argc >= 4)
		timeout = atoi(slash->argv[3]);

	for(int i = 0; i < hostarr_cnt; i++)
		param_group_pull(group, hostarr[i], timeout);

	param_print_header(hostarr, hostarr_cnt);
	for(int i = 0; i < group->count; i++) {
		param_print(group->params[i], hostarr, hostarr_cnt, 0);
	}

	return SLASH_SUCCESS;
}
slash_command_sub(group, pull, group_pull_slash, "<group_name> [node] [timeout]", NULL);

static int group_push_slash(struct slash *slash)
{
	if (slash->argc < 2)
		return SLASH_EUSAGE;

	param_group_t * group = param_group_find_name(slash->argv[1]);
	if (group == NULL)
		return SLASH_EINVAL;

	unsigned int node = csp_get_address();
	unsigned int timeout = 1000;

	if (slash->argc >= 3)
		node = atoi(slash->argv[2]);
	if (slash->argc >= 4)
		timeout = atoi(slash->argv[3]);

	param_group_push(group, node, timeout);

	return SLASH_SUCCESS;
}
slash_command_sub(group, push, group_push_slash, "<group_name> [node] [timeout]", NULL);
