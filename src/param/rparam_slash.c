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
	if (slash->argc != 3)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int id = atoi(slash->argv[2]);

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return SLASH_EINVAL;

	uint16_t * request = packet->data16;
	request[0] = csp_hton16(id);
	packet->length = sizeof(request[0]);

	csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_GET, 0, CSP_SO_NONE);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return SLASH_EINVAL;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return SLASH_EINVAL;
	}

	packet = csp_read(conn, 5000);
	if (packet == NULL) {
		csp_close(conn);
		slash_printf(slash, "No response\n");
		return SLASH_EINVAL;
	}

	csp_hex_dump("Response", packet->data, packet->length);
	csp_buffer_free(packet);
	csp_close(conn);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, get, rparam_slash_get, "<node> <param>", "Get remote parameter");

static int rparam_slash_set(struct slash *slash)
{
	if (slash->argc != 5)
		return SLASH_EUSAGE;

	unsigned int node = atoi(slash->argv[1]);
	unsigned int idx = atoi(slash->argv[2]);
	unsigned int type = atoi(slash->argv[3]);

	//printf("Node %u idx %u type %u\r\n", node, idx, type);

	unsigned int value;
	param_str_to_value(type, slash->argv[4], &value);

	//printf("value %u\r\n", value);

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return SLASH_EINVAL;

	packet->length = 0;
	packet->length += param_serialize_single_fromstr(idx, type, slash->argv[4], (char *) packet->data, 256 - packet->length);

	csp_hex_dump("packet", packet->data, packet->length);

	if (csp_sendto(CSP_PRIO_HIGH, node, PARAM_PORT_SET, 0, CSP_SO_NONE, packet, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

	return SLASH_SUCCESS;
}
slash_command_sub(rparam, set, rparam_slash_set, "<node> <param> <type> <value>", "Set remote parameter");

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



