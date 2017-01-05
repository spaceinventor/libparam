/*
 * param_group.c
 *
 *  Created on: Jan 5, 2017
 *      Author: johan
 */

#include <stdio.h>

#include <param/param_list.h>
#include <param/param_group.h>


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

void param_group_foreach(int (*iterator)(param_group_t * group)) {
	param_group_t * param_group;
	param_group_foreach_static(param_group) {
		if (iterator(param_group) == 0)
			return;
	}
#if 0
	param = list_begin;
	while(param != NULL) {
		if (iterator(param) == 0)
			return;
		param = param->next;
	}
#endif
}

void param_group_iterate(param_group_t *group, int (*iterator)(param_t * param)) {
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
		iterator(param);
	}
}

void param_group_print(void) {
	int group_iterator(param_group_t * group) {
		printf("Group %s", group->name);
		if (group->interval > 0)
			printf(" interval: %u, %u:%u", (unsigned int) group->interval, group->node, group->port);
		printf("\n");
		int param_iterator(param_t * param) {
			param_print(param);
			return 1;
		}
		param_group_iterate(group, param_iterator);
		return 1;
	}
	param_group_foreach(group_iterator);
}

