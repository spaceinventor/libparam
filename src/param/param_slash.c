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

param_queue_t * queue = NULL;

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
	} else {
		*node = -1;
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

static int get(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;

	param_t * param;
	int host = -1;
	int node = -1;
	int offset = -1;
	param_slash_parse(slash->argv[1], &param, &node, &host, &offset);

	if (param == NULL) {
		printf("%s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	if (host != -1) {
		param_pull_single(param, 0, host, 1000);
	}

	param_print(param, offset, NULL, 0, 2);

	return SLASH_SUCCESS;
}
slash_command_completer(get, get, param_completer, "<param>", "Get");

static int set(struct slash *slash)
{
	if (slash->argc != 3)
		return SLASH_EUSAGE;

	param_t * param;
	int host = -1;
	int node = -1;
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

		/* If host is specified, set single now */
		if (host != -1) {
			param_push_single(param, valuebuf, 1, host, 1000);

		/* Add to queue */
		} else {
			if (!queue) {
				queue = param_queue_create(NULL, 256);
			}
			param_queue_add(queue, param, valuebuf);
			param_queue_print(queue);
		}

	/* For local parameters, set immediately */
	} else {
		param_set(param, offset, valuebuf);
	}

	param_print(param, -1, NULL, 0, 2);

	return SLASH_SUCCESS;
}
slash_command_completer(set, set, param_completer, "<param> <value>", "Set");

static int push(struct slash *slash)
{
	unsigned int node = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	param_push_queue(queue, 1, node, timeout);

	return SLASH_SUCCESS;
}
slash_command(push, push, "<node> [timeout]", NULL);

static int pull(struct slash *slash)
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
slash_command(pull, pull, "<node> [timeout]", NULL);

static int clear(struct slash *slash)
{
	if (queue)
		param_queue_destroy(queue);
	queue = NULL;
	printf("Queue cleared\n");
	return SLASH_SUCCESS;
}
slash_command(clear, clear, NULL, NULL);
