/*
 * rparam_listset.c
 *
 *  Created on: Nov 21, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <csp/csp.h>

#include <param/rparam_listset.h>

static rparam_listset_t * listset_begin = NULL;
static rparam_listset_t * listset_end = NULL;

int rparam_listset_add(rparam_listset_t * item) {

	if (listset_begin == NULL)
		listset_begin = item;

	if (listset_end != NULL)
		listset_end->next = item;

	listset_end = item;

	item->next = NULL;

	return 0;

}

rparam_listset_t * rparam_listset_find(char * name) {

	rparam_listset_t * list = listset_begin;
	while(list != NULL) {

		if (strcmp(list->listname, name) == 0)
			return list;

		list = list->next;
	}
	return NULL;
}

void rparam_listset_print(void) {

	rparam_listset_t * list = listset_begin;
		while(list != NULL) {
			printf("\t%s\n", list->listname);
			list = list->next;
		}

}

#if 0
static int rparam_slash_list(struct slash *slash)
{
	int node = csp_get_address();
	int timeout = 2000;
	char * endptr;

	if (slash->argc < 2) {
		printf("Select list:\n");
		rparam_listset_print();
		return SLASH_EUSAGE;
	}

	rparam_listset_t * list = rparam_listset_find(slash->argv[1]);

	if (list == NULL) {
		printf("Could not find list with name: %s\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

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

	printf("Requesting list %s from %u, timeout %u\n", list->listname, node, timeout);

	param_t *rparams[50];
	int rparams_count = 0;
	for (char ** name = list->names; *name != NULL; name++) {
		rparams[rparams_count] = param_list_find_name(node, *name);
		if ((rparams[rparams_count] != NULL) && (rparams_count < 50)) {
			rparams_count++;
		}
	}

	if (rparams_count == 0) {
		return SLASH_EINVAL;
	}

	rparams[0]->node = node;

	param_pull(rparams, rparams_count, 1, timeout);

	return SLASH_SUCCESS;
}

slash_command_sub(rparam, list, rparam_slash_list, "<list> [node] [timeout]", NULL);
#endif
