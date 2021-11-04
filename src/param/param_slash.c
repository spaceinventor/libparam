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

param_queue_t param_queue_set = { };
param_queue_t param_queue_get = { };
static int default_node = -1;
static int autosend = 1;
static int paramver = 2;

void param_slash_parse(char * arg, param_t **param, int *node, int *host,
		int *offset) {

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
	int result = 0;
	if (param->node != PARAM_LIST_LOCAL) {

		if (autosend) {
			if (host != -1) {
				result = param_pull_single(param, offset, 1, host, 1000, paramver);
			} else if (node != -1) {
				result = param_pull_single(param, offset, 1, node, 1000, paramver);
			}
		} else {
			if (!param_queue_get.buffer) {
			    param_queue_init(&param_queue_get, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);
			}
			if (param_queue_add(&param_queue_get, param, offset, NULL) < 0)
				printf("Queue full\n");
			param_queue_print(&param_queue_get);
			return SLASH_SUCCESS;
		}

	} else if (autosend) {
		param_print(param, -1, NULL, 0, 0);
	} else {
		if (!param_queue_get.buffer) {
			param_queue_init(&param_queue_get, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);
		}
		if (param_queue_add(&param_queue_get, param, offset, NULL) < 0)
			printf("Queue full\n");
		param_queue_print(&param_queue_get);
		return SLASH_SUCCESS;
	}

	if (result < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_completer(get, cmd_get, param_completer, "<param>", "Get");

static int cmd_set(struct slash *slash) {
	if (slash->argc != 3)
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

	char valuebuf[128] __attribute__((aligned(16))) = { };
	param_str_to_value(param->type, slash->argv[2], valuebuf);

	/* Remote parameters are sent to a queue or directly */
	int result = 0;
	if (param->node != PARAM_LIST_LOCAL) {

		if ((node != -1) && (autosend)) {
			result = param_push_single(param, offset, valuebuf, 1, node, 1000, paramver);
		} else if (host != -1) {
			result = param_push_single(param, offset, valuebuf, 1, host, 1000, paramver);
		} else {
			if (!param_queue_set.buffer) {
				param_queue_init(&param_queue_set, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
			}
			if (param_queue_add(&param_queue_set, param, offset, valuebuf) < 0)
				printf("Queue full\n");
			param_queue_print(&param_queue_set);
			return SLASH_SUCCESS;
		}
		
	} else if (autosend) {
		/* For local parameters, set immediately if autosend is enabled */
	    if (offset < 0)
			offset = 0;
		param_set(param, offset, valuebuf);
	} else {
		/* If autosend is off, queue the parameters */
		if (!param_queue_set.buffer) {
			param_queue_init(&param_queue_set, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
		}
		if (param_queue_add(&param_queue_set, param, offset, valuebuf) < 0)
			printf("Queue full\n");
		param_queue_print(&param_queue_set);
		return SLASH_SUCCESS;
	}

	if (result < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	param_print(param, -1, NULL, 0, 2);

	return SLASH_SUCCESS;
}
slash_command_completer(set, cmd_set, param_completer, "<param> <value>",
		"Set");

static int cmd_push(struct slash *slash) {
	unsigned int node = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		node = atoi(slash->argv[1]);
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	if (param_push_queue(&param_queue_set, 1, node, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command(push, cmd_push, "<node> [timeout]", NULL);

static int cmd_pull(struct slash *slash) {
	unsigned int host = 0;
	unsigned int timeout = 1000;
	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

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
    param_queue_get.version = paramver;
    param_queue_set.version = paramver;
	printf("Queue cleared\n");
	return SLASH_SUCCESS;
}
slash_command(clear, cmd_clear, NULL, NULL);

static int cmd_node(struct slash *slash) {

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

static int cmd_paramver(struct slash *slash) {

    if (slash->argc < 1) {
        printf("Parameter client version = %d\n", paramver);
    }

    if (slash->argc >= 1) {
        paramver = atoi(slash->argv[1]);
        printf("Set parameter client version to %d\n", paramver);
    }

    return SLASH_SUCCESS;
}
slash_command(paramver, cmd_paramver, "[1|2]", NULL);

static int cmd_autosend(struct slash *slash) {

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
