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

#include <param/param.h>
#include "param_string.h"

slash_command_group(param, "Local parameters");

static int list(struct slash *slash)
{
	param_list(NULL);
	return SLASH_SUCCESS;
}
slash_command_sub(param, list, list, NULL, "List parameters");

static param_t * parse_param(char * arg) {

	char * endptr;
	int idx = strtoul(arg, &endptr, 10);

	if (*endptr != '\0') {
		return param_name_to_ptr(arg);
	} else {
		return param_index_to_ptr(idx);
	}

}

static void param_completer(struct slash *slash, char * token) {

	int matches = 0;
	size_t prefixlen = -1;
	param_t *prefix = NULL;
	size_t tokenlen = strlen(token);

	param_t * param;
	param_foreach(param) {

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
				prefixlen = slash_prefix_length(prefix->name, param->name);
			}

			/* Print newline on first match */
			if (matches == 1)
				slash_printf(slash, "\n");

			/* Print param */
			param_print(param);

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

	param_t * param = parse_param(slash->argv[1]);

	if (param == NULL) {
		printf("Parameter %s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	param_print(param);

	return SLASH_SUCCESS;
}
slash_command_sub_completer(param, get, get, param_completer, "<param>", "Get");

static int set(struct slash *slash)
{
	if (slash->argc != 3)
		return SLASH_EUSAGE;

	param_t * param = parse_param(slash->argv[1]);

	if (param == NULL) {
		printf("Parameter %s not found\n", slash->argv[1]);
		return SLASH_EINVAL;
	}

	char valuebuf[128] __attribute__((aligned(16))) = {};
	param_str_to_value(param->type, slash->argv[2], valuebuf);

	param_set(param, valuebuf);

	param_print(param);

	return SLASH_SUCCESS;
}
slash_command_sub_completer(param, set, set, param_completer, "<param> <value>", "Set");


