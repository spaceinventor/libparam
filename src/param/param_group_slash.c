/*
 * param_group_slash.c
 *
 *  Created on: Jan 5, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <slash/slash.h>

#include <param/param_group.h>

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
