/*
 * rparam_slash.c
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/arch/csp_malloc.h>

#include <param/param.h>
#include <param/rparam.h>
#include <param/rparam_list.h>

#include "param_serializer.h"
#include "param_string.h"

slash_command_group(rparam, "Remote parameters");

static rparam_t * rparams[100];
static int rparams_count = 0;
static int rparam_autosend = 1;

static void rparam_print_queue(void) {
	printf("Queue\n");
	for(int i = 0; i < rparams_count; i++)
		rparam_print(rparams[i]);
}

static int rparam_slash_get(struct slash *slash)
{
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (rparam_get(rparams, rparams_count) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	if (slash->argc < 3)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	rparam_t * rparam = rparam_list_find_name(node, slash->argv[2]);

	if (rparam == NULL) {
		slash_printf(slash, "Could not find parameter\n");
		return SLASH_EINVAL;
	}

	if ((rparams_count > 0) && (rparams[0]->node != rparam->node)) {
		printf("You can only queue parameters from the same node\n");
		return SLASH_EINVAL;
	}

	int already_in_queue = 0;
	for(int i = 0; i < rparams_count; i++) {
		if (rparams[i]->idx == rparam->idx)
			already_in_queue = 1;
	}

	if (!already_in_queue) {
		rparams[rparams_count++] = rparam;
		if (rparam_autosend == 0)
			printf("Added %s to queue\n", rparam->name);
	}

	if (rparam_autosend == 0) {
		rparam_print_queue();
		return SLASH_SUCCESS;
	}

	if (rparam_get(rparams, rparams_count) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	rparams_count = 0;
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, get, rparam_slash_get, "<node> <param> [type] [size]", "Get remote parameter");

static int rparam_slash_set(struct slash *slash)
{
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (rparam_set(rparams, rparams_count) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	if (slash->argc < 3)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	rparam_t * rparam = rparam_list_find_name(node, slash->argv[2]);

	if (rparam == NULL) {
		slash_printf(slash, "Could not find parameter\n");
		return SLASH_EINVAL;
	}

	if ((rparams_count > 0) && (rparams[0]->node != rparam->node)) {
		printf("You can only queue parameters from the same node\n");
		return SLASH_EINVAL;
	}

	if (rparam->setvalue == NULL) {

		/* Allocate storage for parameter data */
		int valuesize = param_typesize(rparam->type);
		if (valuesize == -1) {
			valuesize = rparam->size;
		}

		rparam->setvalue = calloc(valuesize, 1);

	}

	param_str_to_value(rparam->type, slash->argv[3], rparam->setvalue);
	rparam->setvalue_pending = 1;

	int already_in_queue = 0;
	for(int i = 0; i < rparams_count; i++) {
		if (rparams[i]->idx == rparam->idx)
			already_in_queue = 1;
	}

	if (!already_in_queue) {
		rparams[rparams_count++] = rparam;
		if (rparam_autosend == 0)
			printf("Added %s to queue\n", rparam->name);
	}

	if (rparam_autosend == 0) {
		rparam_print_queue();
		return SLASH_SUCCESS;
	}

	if (rparam_set(rparams, rparams_count) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	rparams_count = 0;
	return SLASH_SUCCESS;

}
slash_command_sub(rparam, set, rparam_slash_set, "<node> <param> <value>", NULL);

static int rparam_slash_download(struct slash *slash)
{
	if (slash->argc < 2)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int timeout = 1000;
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	rparam_list_download(node, timeout);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, download, rparam_slash_download, "<node> [timeout]", NULL);

static int rparam_slash_list(struct slash *slash)
{
	int node = -1;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	rparam_list_print(node);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, list, rparam_slash_list, "", "list remote parameters");

static int rparam_slash_clear(struct slash *slash)
{
	rparams_count = 0;
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, clear, rparam_slash_clear, "", NULL);

static int rparam_slash_autosend(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;
	rparam_autosend = atoi(slash->argv[1]);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, autosend, rparam_slash_autosend, "<autosend>", NULL);

static int rparam_slash_queue(struct slash *slash)
{
	rparam_print_queue();
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, queue, rparam_slash_queue, "<autosend>", NULL);

