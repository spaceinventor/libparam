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
#include <param/rparam_list.h>

#include "param_serializer.h"
#include "param_string.h"

slash_command_group(rparam, "Remote parameters");

static rparam_t * rparams[100];
static int rparams_count = 0;
static int rparam_autosend = 1;

static int rparam_slash_get(struct slash *slash)
{
	if (slash->argc < 3)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	rparam_t * rparam = rparam_list_find_name(node, slash->argv[2]);

	if (rparam == NULL) {
		slash_printf(slash, "Could not find parameter\n");
		return SLASH_EINVAL;
	}

	if ((rparams_count > 0) && (rparams[0]->node != rparam->node)) {
		printf("You can only queue parameters from the same node\n");
		return SLASH_EINVAL;
	}

	int already_in_queue = 0;
	for(int i = 0; i < rparams_count; i++) {
		if (rparams[i]->idx == rparam->idx)
			already_in_queue = 1;
	}

	if (!already_in_queue) {
		rparams[rparams_count++] = rparam;
		if (rparam_autosend == 0)
			printf("Added %s to queue\n", rparam->name);
	}

	if (rparams_count > 1) {
		printf("Queue\n");
		for(int i = 0; i < rparams_count; i++)
			printf("  %s\n", rparams[i]->name);
	}

	if (rparam_autosend == 0)
		return SLASH_SUCCESS;

	if (rparam_get(rparams, rparams_count) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}

	rparams_count = 0;
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
		rparam->type = atoi(slash->argv[3]);
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
slash_command_sub(rparam, set, rparam_slash_set, "<node> <param> [type] [size] <value>", NULL);

static int rparam_slash_download(struct slash *slash)
{
	if (slash->argc < 2)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int timeout = 1000;
	if (slash->argc >= 3)
		timeout = atoi(slash->argv[2]);

	rparam_list_download(node, timeout);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, download, rparam_slash_download, "<node> [timeout]", NULL);

static int rparam_slash_list(struct slash *slash)
{
	rparam_list_print();
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, list, rparam_slash_list, "", "list remote parameters");

static int rparam_slash_send(struct slash *slash)
{
	if (rparams_count == 0)
		return SLASH_EINVAL;

	if (rparam_get(rparams, rparams_count) < 0) {
		printf("No response\n");
		return SLASH_EINVAL;
	}
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, send, rparam_slash_send, "", NULL);

static int rparam_slash_clear(struct slash *slash)
{
	rparams_count = 0;
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, clear, rparam_slash_clear, "", NULL);

static int rparam_slash_autosend(struct slash *slash)
{
	if (slash->argc != 2)
		return SLASH_EUSAGE;
	rparam_autosend = atoi(slash->argv[1]);
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, autosend, rparam_slash_autosend, "<autosend>", NULL);


