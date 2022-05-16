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
#include <slash/dflopt.h>

#include <csp/csp.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>

#include "param_string.h"
#include "param_slash.h"
#include "param_wildcard.h"

char queue_set_buf[PARAM_SERVER_MTU];
char queue_get_buf[PARAM_SERVER_MTU];

param_queue_t param_queue_set = { .buffer = queue_set_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_SET, .version = 2 };
param_queue_t param_queue_get = { .buffer = queue_get_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_GET, .version = 2 };

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

	return;

}

static void param_completer(struct slash *slash, char * token) {

	int matches = 0;
	size_t prefixlen = -1;
	param_t *prefix = NULL;
	size_t tokenlen = strlen(token);

	param_t * param;
	param_list_iterator i = { };
	if (has_wildcard(token, strlen(token))) {
		// Only print parameters when globbing is involved.
		while ((param = param_list_iterate(&i)) != NULL)
			if (strmatch(param->name, token, strlen(param->name), strlen(token)))
				param_print(param, -1, NULL, 0, 2);
		return;
	}

	while ((param = param_list_iterate(&i)) != NULL) {

		if (tokenlen > strlen(param->name))
			continue;

		if (strncmp(token, param->name,
				slash_min(strlen(param->name), tokenlen)) == 0) {

			/* Count matches */
			matches++;

			/* Find common prefix */
			if (prefixlen == (size_t) -1) {
				prefix = param;
				prefixlen = strlen(prefix->name);
			} else {
				size_t new_prefixlen = slash_prefix_length(prefix->name,
						param->name);
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

static int cmd_get(struct slash *slash) {
	//if (slash->argc != 2)
	//	return SLASH_EUSAGE;

	param_t * param;
	int host = -1;
	int node = slash_dfl_node;
	int offset = -1;
	int paramver = 2;
	char * name = slash->argv[1];
	param_slash_parse(slash->argv[1], &param, &node, &host, &offset);

	/* Go through the list of parameters */
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		/* Name match (with wildcard) */
		if (strmatch(param->name, name, strlen(param->name), strlen(name)) == 0) {
			continue;
		}

		/* Node match */
		if (param->node != node) {
			continue;
		}

		/* Local parameters are printed directly */
		if ((param->node == 0) && (autosend)) {
			param_print(param, -1, NULL, 0, 0);
			continue;
		}

		/* Queue */
		if (autosend == 0) {
			if (param_queue_add(&param_queue_get, param, offset, NULL) < 0)
				printf("Queue full\n");
			continue;	
		}

		/* Select destination, host overrides parameter node */
		int dest = node;
		if (host != -1)
			dest = host;

		if (param_pull_single(param, offset, 1, dest, slash_dfl_timeout, paramver) < 0) {
			printf("No response\n");
			continue;
		}
		
	}

	if (autosend == 0) {
		param_queue_print(&param_queue_get);
	}

	return SLASH_SUCCESS;

}
slash_command_completer(get, cmd_get, param_completer, "<param>", "Get");

static int cmd_set(struct slash *slash) {
	if (slash->argc != 3)
		return SLASH_EUSAGE;

	param_t * param;
	int host = -1;
	int node = slash_dfl_node;
	int offset = -1;
	int paramver = 2;
	param_slash_parse(slash->argv[1], &param, &node, &host, &offset);

	//printf("set %s node %d host %d\n", param->name, node, host);

	if (param == NULL) {
		printf("%s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	char valuebuf[128] __attribute__((aligned(16))) = { };
	param_str_to_value(param->type, slash->argv[2], valuebuf);

	/* Local parameters are set directly */
	if ((param->node == 0) && autosend) {

		/* Ensure offset is positive for local parametrs */
	    if (offset < 0)
			offset = 0;

		param_set(param, offset, valuebuf);
	}

	if (autosend == 0) {
		if (param_queue_add(&param_queue_set, param, offset, valuebuf) < 0)
			printf("Queue full\n");
		param_queue_print(&param_queue_set);
		return SLASH_SUCCESS;
	}

	/* Select destination, host overrides parameter node */
	int dest = node;
	if (host != -1)
		dest = host;

	if (param_push_single(param, offset, valuebuf, 1, dest, 1000, paramver) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	param_print(param, -1, NULL, 0, 2);

	return SLASH_SUCCESS;
}
slash_command_completer(set, cmd_set, param_completer, "<param> <value>", "Set");

static int cmd_push(struct slash *slash) {
	unsigned int node = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	uint32_t hwid = 0;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);
	if (slash->argc >= 4)
		hwid = atoi(slash->argv[3]);

	if (param_push_queue(&param_queue_set, 1, node, timeout, hwid) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command(push, cmd_push, "<node> [timeout] [hwid]", NULL);

static int cmd_pull(struct slash *slash) {
	unsigned int host = slash_dfl_node;
	unsigned int timeout = slash_dfl_timeout;
	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;
	int paramver = 2;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		host = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		include_mask = param_maskstr_to_mask(slash->argv[2]);
	if (slash->argc >= 4)
	    exclude_mask = param_maskstr_to_mask(slash->argv[3]);
	if (slash->argc >= 5)
		timeout = atoi(slash->argv[4]);

	int result = -1;
	if (param_queue_get.used == 0) {
		result = param_pull_all(1, host, include_mask, exclude_mask, timeout, paramver);
	} else {
		result = param_pull_queue(&param_queue_get, 1, host, timeout);
	}

	if (result) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command(pull, cmd_pull, "<node> [mask] [timeout]", NULL);

static int cmd_clear(struct slash *slash) {
	param_queue_get.used = 0;
	param_queue_set.used = 0;
	printf("Queue cleared\n");
	return SLASH_SUCCESS;
}
slash_command(clear, cmd_clear, NULL, NULL);

static int cmd_autosend(struct slash *slash) {

	int paramver = 2;

	if (slash->argc < 1) {
		printf("autosend = %d\n", autosend);
	}

	if (slash->argc >= 1) {
		autosend = atoi(slash->argv[1]);
		printf("Set autosend to %d\n", autosend);
	}

	param_queue_get.used = 0;
	param_queue_get.version = paramver;
	param_queue_set.used = 0;
	param_queue_set.version = paramver;

	return SLASH_SUCCESS;
}
slash_command(autosend, cmd_autosend, "[1|0]", NULL);

static int cmd_queue(struct slash *slash) {
	if ( (param_queue_get.used == 0) && (param_queue_set.used == 0) ) {
		printf("Nothing queued\n");
	}
	if (param_queue_get.used > 0) {
		printf("Get Queue\n");
		param_queue_print(&param_queue_get);
	}
	if (param_queue_set.used > 0) {
		printf("Set Queue\n");
		param_queue_print(&param_queue_set);
	}
	return SLASH_SUCCESS;
}
slash_command(queue, cmd_queue, NULL, NULL);
