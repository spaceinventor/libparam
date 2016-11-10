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

#define RPARAM_SLASH_MAX_QUEUESIZE 500

static rparam_t * rparams[RPARAM_SLASH_MAX_QUEUESIZE];
static int rparams_count = 0;
static int rparam_autosend = 1;

static void rparam_print_queue(void) {
	printf("Queue\n");
	for(int i = 0; i < rparams_count; i++)
		rparam_print(rparams[i]);
}

static int rparam_slash_get(struct slash *slash)
{

	/**
	 * If called without arguments, and there are stuff in queue, get now
	 */
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (rparam_get(rparams, rparams_count) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	/**
	 * If called with a node, and the queue is clear, create the queue automatically
	 */
	if ((slash->argc == 2) && (rparams_count == 0)) {

		printf("Autogenerating queue from all parameters\n");

		unsigned int node = atoi(slash->argv[1]);

		void add_to_queue(rparam_t * rparam) {
			if (rparam->node == node) {
				if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
					rparams[rparams_count++] = rparam;
			}
		}
		rparam_list_foreach(add_to_queue);

		rparam_print_queue();
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
		if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
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

	/**
	 * If called without arguments, and there are stuff in queue, set now
	 */
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (rparam_set(rparams, rparams_count) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	/**
	 * If called with a node, and the queue is clear, create the queue automatically
	 */
	if ((slash->argc == 2) && (rparams_count == 0)) {

		printf("Autogenerating queue from pending parameters\n");

		unsigned int node = atoi(slash->argv[1]);

		void add_to_queue(rparam_t * rparam) {
			if (rparam->node == node && rparam->setvalue_pending == 1) {
				if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
					rparams[rparams_count++] = rparam;
			}
		}
		rparam_list_foreach(add_to_queue);

		rparam_print_queue();
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
		rparam->setvalue = calloc(rparam_size(rparam), 1);
	}

	param_str_to_value(rparam->type, slash->argv[3], rparam->setvalue);
	rparam->setvalue_pending = 1;

	int already_in_queue = 0;
	for(int i = 0; i < rparams_count; i++) {
		if (rparams[i]->idx == rparam->idx)
			already_in_queue = 1;
	}

	if (!already_in_queue) {
		if (rparams_count < RPARAM_SLASH_MAX_QUEUESIZE)
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
	int pending = 0;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		pending = atoi(slash->argv[2]);
	rparam_list_print(node, pending);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, list, rparam_slash_list, "<node> <pending>", "list remote parameters");

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

