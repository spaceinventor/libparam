/*
 * param_slash.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <csp/csp.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>

#include "param_string.h"
#include "param_slash.h"

static param_queue_t * queue_set = NULL;
static param_queue_t * queue_get = NULL;
static int default_node = 1;
static int autosend = 1;

void param_slash_parse(char * arg, param_t **param, int *node, int *host, int *offset) {

	/* Search for the '@' symbol:
	 * Call strtok twice in order to skip the stuff head of '@' */
	char * saveptr;
	char * token;

	strtok_r(arg, "@", &saveptr);
	token = strtok_r(NULL, "@", &saveptr);
	if (token != NULL) {
		sscanf(token, "%d", host);
		*token = '\0';
	} else {
		*host = -1;
	}

	/* Search for the ':' symbol: */
	strtok_r(arg, ":", &saveptr);
	token = strtok_r(NULL, ":", &saveptr);
	if (token != NULL) {
		sscanf(token, "%d", node);
		*token = '\0';
	} else if (*host != -1) {
		*node = *host;
	}

	/* Search for the '[' symbol: */
	strtok_r(arg, "[", &saveptr);
	token = strtok_r(NULL, "[", &saveptr);
	if (token != NULL) {
		sscanf(token, "%d", offset);
		*token = '\0';
	}

	char *endptr;
	int id = strtoul(arg, &endptr, 10);

	if (*endptr == '\0') {
		*param = param_list_find_id(*node, id);
	} else {
		*param = param_list_find_name(*node, arg);
	}

	/* If host is set try to serch in templates */
	if ((*param == NULL) && (*host != -1)) {
		if (*endptr == '\0') {
			*param = param_list_find_id(-2, id);
		} else {
			*param = param_list_find_name(-2, arg);
		}

		if (*param) {
			*param = param_list_template_to_param(*param, *host);
			*node = *host;
		}

	}

	return;

}

static void param_completer(struct slash *slash, char * token) {

	int matches = 0;
	size_t prefixlen = -1;
	param_t *prefix = NULL;
	size_t tokenlen = strlen(token);

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if (tokenlen > strlen(param->name))
			continue;

		if (param->readonly == PARAM_HIDDEN)
			continue;

		if (strncmp(token, param->name, slash_min(strlen(param->name), tokenlen)) == 0) {

			/* Count matches */
			matches++;

			/* Find common prefix */
			if (prefixlen == (size_t) -1) {
				prefix = param;
				prefixlen = strlen(prefix->name);
			} else {
				size_t new_prefixlen = slash_prefix_length(prefix->name, param->name);
				if (new_prefixlen < prefixlen)
					prefixlen = new_prefixlen;
			}

			/* Print newline on first match */
			if (matches == 1)
				slash_printf(slash, "\n");

			/* Print param */
			param_print(param, -1, NULL, 0, 2);

		}

	}

	if (!matches) {
		slash_bell(slash);
	} else {
		strncpy(token, prefix->name, prefixlen);
		slash->cursor = slash->length = (token - slash->buffer) + prefixlen;
	}

}

static int cmd_get(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	param_t * param;
	int host = -1;
	int node = default_node;
	int offset = -1;
	param_slash_parse(slash->argv[1], &param, &node, &host, &offset);

	if (param == NULL) {
		printf("%s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	/* Remote parameters are sent to a queue or directly */
	if (param->storage_type == PARAM_STORAGE_REMOTE) {


		if ((node != -1) && (autosend)) {
			param_pull_single(param, 0, node, 1000);
		} else if (host != -1) {
			param_pull_single(param, 0, host, 1000);
		} else {
			if (!queue_get) {
				queue_get = param_queue_create(NULL, 256, PARAM_QUEUE_TYPE_GET);
			}
			param_queue_add(queue_get, param, NULL);
			param_queue_print(queue_get);
			return SLASH_SUCCESS;
		}

	}

	param_print(param, offset, NULL, 0, 2);

	return SLASH_SUCCESS;
}
slash_command_completer(get, cmd_get, param_completer, "<param>", "Get");

static int cmd_set(struct slash *slash)
{
	if (slash->argc != 3)
		return SLASH_EUSAGE;

	param_t * param;
	int host = -1;
	int node = default_node;
	int offset = 0;
	param_slash_parse(slash->argv[1], &param, &node, &host, &offset);

	if (param == NULL) {
		printf("%s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	char valuebuf[128] __attribute__((aligned(16))) = {};
	param_str_to_value(param->type, slash->argv[2], valuebuf);

	/* Remote parameters are sent to a queue or directly */
	if (param->storage_type == PARAM_STORAGE_REMOTE) {

		if ((node != -1) && (autosend)) {
			param_push_single(param, valuebuf, 1, node, 1000);
		} else if (host != -1) {
			param_push_single(param, valuebuf, 1, host, 1000);
		} else {
			if (!queue_set) {
				queue_set = param_queue_create(NULL, 256, PARAM_QUEUE_TYPE_SET);
			}
			param_queue_add(queue_set, param, valuebuf);
			param_queue_print(queue_set);
		}

	/* For local parameters, set immediately */
	} else {
		param_set(param, offset, valuebuf);
	}

	param_print(param, -1, NULL, 0, 2);

	return SLASH_SUCCESS;
}
slash_command_completer(set, cmd_set, param_completer, "<param> <value>", "Set");

static int cmd_push(struct slash *slash)
{
	unsigned int node = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	param_push_queue(queue_set, 1, node, timeout);

	return SLASH_SUCCESS;
}
slash_command(push, cmd_push, "<node> [timeout]", NULL);

static int cmd_pull(struct slash *slash)
{
	unsigned int node = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	param_queue_print(queue_get);

#if 0
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
#endif

	return SLASH_SUCCESS;
}
slash_command(pull, cmd_pull, "<node> [timeout]", NULL);

static int cmd_clear(struct slash *slash)
{
	if (queue_get) {
		param_queue_destroy(queue_get);
		queue_get = NULL;
	}
	if (queue_set) {
		param_queue_destroy(queue_set);
		queue_set = NULL;
	}

	printf("Queue cleared\n");
	return SLASH_SUCCESS;
}
slash_command(clear, cmd_clear, NULL, NULL);

static int cmd_node(struct slash *slash)
{

	if (slash->argc < 1) {
		printf("Default node = %d\n", default_node);
	}

	if (slash->argc >= 1) {
		default_node = atoi(slash->argv[1]);
		printf("Set default node to %d\n", default_node);
	}

	return SLASH_SUCCESS;
}
slash_command(node, cmd_node, "[node]", NULL);

static int cmd_autosend(struct slash *slash)
{

	if (slash->argc < 1) {
		printf("autosend = %d\n", autosend);
	}

	if (slash->argc >= 1) {
		autosend = atoi(slash->argv[1]);
		printf("Set autosend to %d\n", autosend);
	}

	return SLASH_SUCCESS;
}
slash_command(autosend, cmd_autosend, "[1|0]", NULL);

static int cmd_queue(struct slash *slash)
{
	if (queue_get) {
		printf("Get Queue\n");
		param_queue_print(queue_get);
	}
	if (queue_set) {
		printf("Set Queue\n");
		param_queue_print(queue_set);
	}
	return SLASH_SUCCESS;
}
slash_command(queue, cmd_queue, NULL, NULL);
