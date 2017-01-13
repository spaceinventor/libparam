/*
 * param_group.c
 *
 *  Created on: Jan 5, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <param/param_list.h>
#include <param/param_group.h>

#include <sys/queue.h>

/**
 * GNU Linker symbols. These will be autogenerate by GCC when using
 * __attribute__((section("groups"))
 */
extern param_group_t __start_groups, __stop_groups;

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

param_group_t * param_group_iterate(group_iterator_t * iterator) {

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

void param_group_add_param(param_group_t *group, param_t *param) {

	/* Only possible to add to dynamic storage */
	if (group->storage_dynamic == 0)
		return;

	if (group->count >= group->storage_max_count)
		return;

	/* Add to list */
	group->params[group->count++] = param;

}

param_group_t * param_group_find_name(char * name) {
	param_group_t * group;
	group_iterator_t i = {};
	while((group = param_group_iterate(&i)) != NULL) {
		if (strcmp(group->name, name) == 0)
			return group;
	}
	return NULL;
}
