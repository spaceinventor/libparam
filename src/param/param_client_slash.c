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
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>

#include "param_serializer.h"
#include "param_string.h"

static int param_client_slash_push(struct slash *slash)
{
	unsigned int node = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	/* Clear queue first */
	param_t * params[100];
	int params_count = 0;

	int add_to_queue(param_t * param) {
		if (param->node == node && param->value_pending == 1) {
			if (params_count < 100)
				params[params_count++] = param;
		}
		return 1;
	}
	param_list_foreach(add_to_queue);

	if (params_count == 0)
		return SLASH_SUCCESS;

	if (param_push(params, params_count, 1, node, timeout) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(param, push, param_client_slash_push, "<node> [timeout]", NULL);

static int param_client_slash_pull(struct slash *slash)
{
	unsigned int node = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	/* Clear queue first */
	param_t * params[100];
	int params_count = 0;
	int response_size = 0;

	void send_queue(void) {
		param_pull(params, params_count, 1, node, timeout);
		params_count = 0;
		response_size = 0;
	}

	int add_to_queue(param_t * param) {
		if (param->node != node)
			return 1;

		int param_packed_size = sizeof(uint16_t) + param_size(param);
		if (response_size + param_packed_size >= PARAM_SERVER_MTU) {
			send_queue();
		}

		params[params_count++] = param;
		response_size += param_packed_size;
		return 1;
	}
	param_list_foreach(add_to_queue);

	if (params_count == 0)
		return SLASH_SUCCESS;

	send_queue();

	return SLASH_SUCCESS;
}
slash_command_sub(param, pull, param_client_slash_pull, "<node> [timeout]", NULL);


#if 0
static int rparam_slash_node(struct slash *slash)
{
	if (slash->argc < 2) {
		slash_printf(slash, "node: %u, timeout: %u\n", rparam_default_node, rparam_default_timeout);
		return SLASH_EUSAGE;
	}

	rparam_default_node = atoi(slash->argv[1]);

	if (slash->argc >= 3)
		rparam_default_timeout = atoi(slash->argv[2]);

	slash_printf(slash, "Set node to: %u, timeout: %u\n", rparam_default_node, rparam_default_timeout);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, node, rparam_slash_node, "<node> [timeout]", NULL);



static int rparam_slash_get(struct slash *slash)
{

	/**
	 * If called without arguments, and there are stuff in queue, get now
	 */
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (param_pull(rparams, rparams_count, 1, rparam_default_timeout) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	if (slash->argc < 2)
		return SLASH_EUSAGE;

	param_t * rparam = param_list_find_name(rparam_default_node, slash->argv[1]);

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
		if (rparams[i]->id == rparam->id)
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

	if (param_pull(rparams, rparams_count, 1, rparam_default_timeout) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	rparams_count = 0;
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, get, rparam_slash_get, "<param>", NULL);

static int rparam_slash_set(struct slash *slash)
{

	/**
	 * If called without arguments, and there are stuff in queue, set now
	 */
	if ((slash->argc <= 1) && (rparams_count > 0)) {

		if (param_push(rparams, rparams_count, 1, rparam_default_timeout) < 0) {
			printf("No response\n");
			return SLASH_EINVAL;
		}

		return SLASH_SUCCESS;
	}

	if (slash->argc < 3)
		return SLASH_EUSAGE;

	param_t * rparam = param_list_find_name(rparam_default_node, slash->argv[1]);

	if (rparam == NULL) {
		slash_printf(slash, "Could not find parameter\n");
		return SLASH_EINVAL;
	}

	if ((rparams_count > 0) && (rparams[0]->node != rparam->node)) {
		printf("You can only queue parameters from the same node\n");
		return SLASH_EINVAL;
	}

	if (rparam->value_set != NULL) {
		param_str_to_value(rparam->type, slash->argv[2], rparam->value_set);
		rparam->value_pending = 1;
	}

	int already_in_queue = 0;
	for(int i = 0; i < rparams_count; i++) {
		if (rparams[i]->id == rparam->id)
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

	if (param_push(rparams, rparams_count, 1, rparam_default_timeout) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	rparams_count = 0;
	return SLASH_SUCCESS;

}
slash_command_sub(rparam, set, rparam_slash_set, "<param> <value>", NULL);


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

#endif
