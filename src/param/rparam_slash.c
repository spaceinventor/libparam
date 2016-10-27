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
#include <param/rparam.h>
#include "param_serializer.h"
#include "param_string.h"
#include "rparam_list.h"

slash_command_group(rparam, "Remote parameters");

static int rparam_slash_get(struct slash *slash)
{
	if (slash->argc < 3)
		return SLASH_EUSAGE;

	rparam_t * rparam = NULL;
	unsigned int node = atoi(slash->argv[1]);
	unsigned int idx;

	char * endptr;
	idx = strtoul(slash->argv[2], &endptr, 10);
	if (*endptr != '\0') {
		rparam = rparam_list_find_name(node, slash->argv[2]);
		if (rparam == NULL)
			return SLASH_EINVAL;
	} else {
		if (slash->argc < 4)
			return SLASH_EINVAL;
		rparam = alloca(sizeof(rparam_t));
		rparam->node = node;
		rparam->idx = idx;
		rparam->timeout = 2000;
		strcpy(rparam->name, "");
		rparam->type = (uint8_t) atoi(slash->argv[3]);
		if (rparam->type == PARAM_TYPE_DATA || rparam->type == PARAM_TYPE_STRING) {
			if (slash->argc < 5)
				return SLASH_EINVAL;
			rparam->size = atoi(slash->argv[4]);
		}
	}

	if (!rparam) {

	}

	int size = param_typesize(rparam->type);
	if (size < 0)
		size = rparam->size;
	if (size == UINT8_MAX)
		return SLASH_EINVAL;

	__attribute__((aligned((8)))) char data[size];
	memset(data, 0, size);
	if (rparam_get(rparam, data) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	if ((rparam->type != PARAM_TYPE_DATA) && (rparam->type != PARAM_TYPE_STRING)) {
		char value_str[20] = {};
		param_var_str(rparam->type, rparam->size, data, value_str, 20);
		printf(" %u %u", rparam->node, rparam->idx);
		if (strlen(rparam->name) != 0)
			printf(" %s", rparam->name);
		printf(" = %s\n", value_str);
	} else {
		csp_hex_dump("Data", data, size);
	}

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, get, rparam_slash_get, "<node> <param> [type] [size]", "Get remote parameter");

static int rparam_slash_set(struct slash *slash)
{
	if (slash->argc < 3)
		return SLASH_EUSAGE;

	rparam_t * rparam = NULL;
	unsigned int node = atoi(slash->argv[1]);
	unsigned int idx;
	char * strarg;

	char * endptr;
	idx = strtoul(slash->argv[2], &endptr, 10);

	if (*endptr != '\0') {
		/* String */
		rparam = rparam_list_find_name(node, slash->argv[2]);
		if (rparam == NULL)
			return SLASH_EINVAL;
		strarg = slash->argv[3];
	} else {
		if (slash->argc < 5)
			return SLASH_EUSAGE;
		strarg = slash->argv[4];
		rparam = alloca(sizeof(rparam_t));
		rparam->node = node;
		rparam->idx = idx;
		rparam->type = atoi(slash->argv[3]);;
		if (rparam->type == PARAM_TYPE_DATA || rparam->type == PARAM_TYPE_STRING) {
			if (slash->argc < 6)
				return SLASH_EINVAL;
			rparam->size = atoi(slash->argv[4]);
			strarg = slash->argv[5];
		}
	}

	printf("Node %u idx %u type %u size %u\r\n", rparam->node, rparam->idx, rparam->type, rparam->size);

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return SLASH_EINVAL;

	packet->length = 0;

	int maxlength = 256 - packet->length;
	if (rparam->size < maxlength)
		maxlength = rparam->size;

	packet->length += param_serialize_single_fromstr(rparam->idx, rparam->type, strarg, (char *) packet->data, maxlength);

	csp_hex_dump("packet", packet->data, packet->length);

	if (csp_sendto(CSP_PRIO_HIGH, node, PARAM_PORT_SET, 0, CSP_SO_NONE, packet, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, set, rparam_slash_set, "<node> <param> [type] [size] <value>", "Set remote parameter");

static int rparam_slash_download(struct slash *slash)
{
	if (slash->argc != 3)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int timeout = atoi(slash->argv[2]);

	rparam_list_download(node, timeout);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, download, rparam_slash_download, "<node> <timeout>", "download remote parameters");

static int rparam_slash_list(struct slash *slash)
{
	rparam_list_foreach();
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, list, rparam_slash_list, "", "list remote parameters");



