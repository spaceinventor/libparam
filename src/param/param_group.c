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

/* Convenient macro to loop over the parameter list */
#define param_group_foreach_static(_c) \
	for (_c = &__start_groups; \
	     _c < &__stop_groups; \
	     _c = (param_group_t *)(intptr_t)((char *)_c + GROUP_STORAGE_SIZE))

static SLIST_HEAD(param_groups_head_s, param_group_s) param_groups_head = {};

int param_group_add(param_group_t * item) {
	SLIST_INSERT_HEAD(&param_groups_head, (struct param_group_s *) item, next);
	return 0;
}

param_group_t * param_group_create(char * name) {
	param_group_t * group = calloc(sizeof(param_group_t), 1);
	if (group == NULL)
		return NULL;
	strncpy(group->name, name, 10);
	param_group_add(group);
	return group;
}

typedef struct group_iterator_s {
	int phase;							// Hybrid iterator has multiple phases (0 == Static, 1 == Dynamic List)
	param_group_t * element;
} group_iterator_t;

static param_group_t * param_groups_iterate(group_iterator_t * iterator) {

	/* First element */
	if (iterator->element == NULL) {
		iterator->element = &__start_groups;
		return iterator->element;
	}

	/* Static phase */
	if (iterator->phase == 0) {

		/* Increment in static memory */
		iterator->element = (param_group_t *)(intptr_t)((char *)iterator->element + GROUP_STORAGE_SIZE);
		if (iterator->element < &__stop_groups)
			return iterator->element;

		/* Switch to dynamic phase */
		iterator->phase = 1;
		iterator->element = SLIST_FIRST(&param_groups_head);
		return iterator->element;
	}

	/* Dynamic phase */
	if (iterator->phase == 1) {
		iterator->element = SLIST_NEXT(iterator->element, next);
		return iterator->element;
	}

	return NULL;

}

static void param_group_print_params(param_group_t *group) {
	for (int i = 0; i < group->count; i++) {
		param_t * param = NULL;
		if (group->ids) {
			int node = group->ids[i] >> 11;
			int id = group->ids[i] & 0x7FF;
			param = param_list_find_id(node, id);
		} if (group->params) {
			param = group->params[i];
		}
		if (param == NULL)
			continue;
		param_print(param);
	}
}

void param_group_print(void) {
	param_group_t * group;
	group_iterator_t i = {};
	while((group = param_groups_iterate(&i)) != NULL) {
		printf("Group %s", group->name);
		if (group->interval > 0)
			printf(" interval: %u, %u:%u", (unsigned int) group->interval, group->node, group->port);
		printf("\n");

		param_group_print_params(group);
	}
}

param_group_t * param_group_find_name(char * name) {
	param_group_t * group;
	group_iterator_t i = {};
	while((group = param_groups_iterate(&i)) != NULL) {
		if (strcmp(group->name, name) == 0)
			return group;
	}
	return NULL;
}


