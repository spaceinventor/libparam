/*
 * param_list_slash.c
 *
 *  Created on: Dec 14, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <param/param.h>
#include <param/param_list.h>
#include "param_list.h"
#include "../param_slash.h"

static int list(struct slash *slash)
{
	if (slash->argc > 1)
		param_list_print(slash->argv[1]);
	else
		param_list_print(NULL);
	return SLASH_SUCCESS;
}
slash_command(list, list, "[str]", "List parameters");

static int list_download(struct slash *slash)
{
	if (slash->argc < 2)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int timeout = 1000;
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	param_list_download(node, timeout);

	return SLASH_SUCCESS;
}
slash_command_sub(list, download, list_download, "<node> [timeout]", NULL);

static int list_str(struct slash *slash) {
	int node_filter = -1;
	if (slash->argc >= 2)
		node_filter = atoi(slash->argv[1]);

	param_list_to_string(stdout, node_filter, 0);
	return SLASH_SUCCESS;
}

slash_command_sub(list, str, list_str, "<node_filter>", NULL);

static int list_refresh(struct slash *slash) {

	if (slash->argc < 3)
		return SLASH_EUSAGE;

	param_t * param = NULL;
	int host = -1;
	int node = -1;

	param_slash_parse(slash->argv[1], &param, &node, &host);

	if (param == NULL)
		return SLASH_EINVAL;

	if (param->storage_type != PARAM_STORAGE_REMOTE)
		return SLASH_EINVAL;

	unsigned int refresh_ms = atoi(slash->argv[2]);

	param->refresh = refresh_ms;
	printf("Set %s refresh %u ms\n", param->name, refresh_ms);

	return SLASH_SUCCESS;
}
slash_command_sub(list, refresh, list_refresh, "<param> <refresh_ms>", NULL);