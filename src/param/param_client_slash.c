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

#if 0
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

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		if (param->node == node && param->value_pending == 1) {
			if (params_count < 100)
				params[params_count++] = param;
		}
	}

	if (params_count == 0)
		return SLASH_SUCCESS;

#if 0
	if (param_push(params, params_count, 1, node, timeout) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}
#endif

	return SLASH_SUCCESS;
}
slash_command(push, param_client_slash_push, "<node> [timeout]", NULL);
#endif

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

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if (param->node != node)
			continue;

		int param_packed_size = sizeof(uint16_t) + param_size(param);
		if (response_size + param_packed_size >= PARAM_SERVER_MTU) {
			send_queue();
		}

		params[params_count++] = param;
		response_size += param_packed_size;
	}

	if (params_count == 0)
		return SLASH_SUCCESS;

	send_queue();

	return SLASH_SUCCESS;
}
slash_command(pull, param_client_slash_pull, "<node> [timeout]", NULL);
