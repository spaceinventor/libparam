/*
 * param_group.c
 *
 *  Created on: Jan 5, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <sys/queue.h>
#include <malloc.h>

#include <csp/csp.h>

#include <param/param_list.h>
#include <param/param_group.h>
#include <param/param_client.h>
#include "param_group.h"

/**
 * The storage size (i.e. how closely two param_t structs are packed in memory)
 * varies from platform to platform (in example on x64 and arm32). This macro
 * defines two param_t structs and saves the storage size in a define.
 */
#ifndef GROUP_STORAGE_SIZE
static const param_group_t param_size_set[2] __attribute__((aligned(1)));
#define GROUP_STORAGE_SIZE ((intptr_t) &param_size_set[1] - (intptr_t) &param_size_set[0])
#endif

static SLIST_HEAD(param_group_head_s, param_group_s) param_group_head = {};

param_group_t * param_group_iterate(param_group_iterator * iterator) {

	/**
	 * GNU Linker symbols. These will be autogenerate by GCC when using
	 * __attribute__((section("groups"))
	 */
	extern param_group_t __start_groups, __stop_groups;

	/* First element */
	if (iterator->element == NULL) {
		iterator->element = &__start_groups;
		return iterator->element;
	}

	/* Static phase */
	if (iterator->phase == 0) {

		/* Increment in static memory */
		iterator->element = (param_group_t *)(intptr_t)((char *)iterator->element + GROUP_STORAGE_SIZE);

		/* Check if we are still within the bounds of the static memory area */
		if (iterator->element < &__stop_groups)
			return iterator->element;

		/* Otherwise, switch to dynamic phase */
		iterator->phase = 1;
		iterator->element = SLIST_FIRST(&param_group_head);
		return iterator->element;
	}

	/* Dynamic phase */
	if (iterator->phase == 1) {
		iterator->element = SLIST_NEXT(iterator->element, next);
		return iterator->element;
	}

	return NULL;

}

param_group_t * param_group_create(char * name, int max_count) {
	param_group_t * group = calloc(sizeof(param_group_t), 1);
	if (group == NULL)
		return NULL;
	strncpy(group->name, name, 10);
	group->storage_dynamic = 1;
	group->storage_max_count = max_count;
	group->params = calloc(max_count * sizeof(uint16_t), 1);
	SLIST_INSERT_HEAD(&param_group_head, group, next);
	return group;
}

param_group_t * param_group_find_name(char * name) {
	param_group_t * group;
	param_group_iterator i = {};
	while((group = param_group_iterate(&i)) != NULL) {
		if (strcmp(group->name, name) == 0)
			return group;
	}
	return NULL;
}

void param_group_param_add(param_group_t *group, param_t *param) {

	/* Only possible to add to dynamic storage */
	if (group->storage_dynamic == 0)
		return;

	if (group->count >= group->storage_max_count)
		return;

	/* Only add if unique */
	for (int i = 0; i < group->count; i++) {
		if (group->params[i] == param)
			return;
	}

	/* Add to list */
	group->params[group->count++] = param;

}

int param_group_push(param_group_t * group, int host, int timeout) {
	return param_push(group->params, group->count, 1, host, timeout);
}

int param_group_pull(param_group_t * group, int host, int timeout) {
	return param_pull(group->params, group->count, 1, host, timeout);
}

void param_group_from_string(FILE *stream) {

	char line[100];
	param_group_t * group = NULL;
	while(fgets(line, 100, stream) != NULL) {

		if (strlen(line) == 0)
			return;

		/* Group */
		if (line[0] == '+') {

			/* Forget about the previous group */
			group = NULL;

			/* Scan line */
			char name[11];
			int interval, node, port;
			int scanned = sscanf(line, "+%10[^#]#%u@%u:%u%*s", name, &interval, &node, &port);
			if (scanned != 4)
				continue;

			/* Search for existing group with that name */
			group = param_group_find_name(name);
			if (group)
				continue;

			/* Otherwise, create a new group */
			group = param_group_create(name, 100);
			group->interval = interval;
			group->node = node;
			group->port = port;
			//printf("Created group %s\n", group->name);
		}

		/* Parameter */
		else if (line[0] == '-') {

			/* Don't parse parameters if no group is set fist */
			if ((group == NULL) || (group->storage_dynamic == 0))
				continue;

			/* Scan line */
			char name[26];
			int id, node;
			int scanned = sscanf(line, "-%25[^|]|%u:%u%*s", name, &id, &node);
			if (scanned != 3)
				continue;

			/* Search for parameter in list */
			param_t *param = param_list_find_id(node, id);
			if (param == NULL)
				continue;

			/* Add parameter to group */
			param_group_param_add(group, param);
			//printf("Added param %s\n", param->name);

		}
	}
}

void param_group_to_string(FILE * stream, char * group_name) {

	param_group_t * group;
	param_group_iterator i = {};
	while((group = param_group_iterate(&i)) != NULL) {

		if (group->storage_dynamic == 0)
			continue;

		fprintf(stream, "+%s#%u@%u:%u\n", group->name, (unsigned int) group->interval, (unsigned int) group->node, (unsigned int) group->port);

		for (int i = 0; i < group->count; i++) {
			param_t * param = group->params[i];
			int node = param->node;
			if (node == PARAM_LIST_LOCAL)
				node = csp_get_address();
			fprintf(stream, "-%s|%u:%u\n", param->name, param->id, node);
		}
	}

}
