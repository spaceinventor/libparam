/*
 * param_slash.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
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
slash_command_sub(param, get, get, "<param>", "Get");

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
slash_command_sub(param, set, set, "<param> <value>", "Set");


