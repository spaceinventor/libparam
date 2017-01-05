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

#include "param_string.h"
#include "param_slash.h"

void param_slash_parse(char * arg, param_t **param, int *node, int *host) {

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

	int iterator(param_t * param) {

		if (tokenlen > strlen(param->name))
			return 1;

		if (param->readonly == PARAM_HIDDEN)
			return 1;

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
			param_print(param);

		}

		return 1;

	}
	param_list_foreach(iterator);

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
	param_slash_parse(slash->argv[1], &param, &node, &host);

	if (param == NULL) {
		printf("Parameter %s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	if (host != -1) {
		param_pull_single(param, host, 1000);
	}

	param_print(param);

	return SLASH_SUCCESS;
}
slash_command_completer(get, get, param_completer, "<param>", "Get");

static int set(struct slash *slash)
{
	if (slash->argc != 3)
		return SLASH_EUSAGE;

	param_t * param;
	int host;
	int node;
	param_slash_parse(slash->argv[1], &param, &node, &host);

	if (param == NULL) {
		printf("Parameter %s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	char valuebuf[128] __attribute__((aligned(16))) = {};
	param_str_to_value(param->type, slash->argv[2], valuebuf);

	param_set(param, valuebuf);

	if (host != -1) {
		param_push_single(param, host, 1000);
	}

	param_print(param);

	return SLASH_SUCCESS;
}
slash_command_completer(set, set, param_completer, "<param> <value>", "Set");


