/*
 * param_client.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>

#include "param_serializer.h"
#include "param_string.h"

int param_pull_single(param_t * param, int host, int timeout) {
	param_t * params[1] = { param };
	return param_pull(params, 1, 0, host, timeout);
}

int param_pull(param_t * params[], int count, int verbose, int host, int timeout) {

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	uint16_t * request = packet->data16;

	int response_size = 0;

	int i;
	for (i = 0; i < count; i++) {
		if (response_size + sizeof(uint16_t) + param_size(params[i]) > PARAM_SERVER_MTU) {
			printf("Request cropped: > MTU\n");
			break;
		}

		response_size += sizeof(uint16_t) + param_size(params[i]);

		int node = params[i]->node;
		if (node == PARAM_LIST_LOCAL)
			node = csp_get_address();

		request[i] = csp_hton16((node << 11) | (params[i]->id & 0x7FF));
	}
	packet->length = sizeof(uint16_t) * i;

	//csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_GET, 0, CSP_O_CRC32);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return -1;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -1;
	}

	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		csp_close(conn);
		return -1;
	}

	//csp_hex_dump("Response", packet->data, packet->length);

	i = 0;
	while(i < packet->length) {

		/* Get id */
		uint16_t id;
		memcpy(&id, &packet->data[i], sizeof(id));
		i += sizeof(id);
		id = csp_ntoh16(id);

		/* Search for param using list */
		param_t * param = param_list_find_id(id >> 11, id & 0x7FF);

		if (param == NULL) {
			printf("No param for node %u id %u\n", id >> 11, id & 0x7FF);
			csp_buffer_free(packet);
			csp_close(conn);
			return -1;
		}

		if (param->value_get == NULL) {
			printf("No memory allocated\n");
			csp_buffer_free(packet);
			csp_close(conn);
			return -1;
		}

		i += param_deserialize_to_var(param->type, param->size, &packet->data[i], param->value_get);

		param->value_updated = csp_get_ms();
		if (param->value_pending == 2)
			param->value_pending = 0;

		if (verbose)
			param_print(param);

	}

	csp_buffer_free(packet);
	csp_close(conn);

	return 0;

}

int param_push_single(param_t * param, int host, int timeout) {
	param_t * params[1] = { param };
	return param_push(params, 1, 0, host, timeout);
}

int param_push(param_t * params[], int count, int verbose, int host, int timeout) {

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	packet->length = 0;
	for (int i = 0; i < count; i++) {

		if ((params[i]->value_set == NULL) || (params[i]->value_pending != 1))
			continue;

		if (packet->length + sizeof(uint16_t) + param_size(params[i]) > PARAM_SERVER_MTU) {
			printf("Request cropped: > MTU\n");
			break;
		}

		/* Parameter id */
		uint16_t id = csp_hton16((params[i]->node << 11) | (params[i]->id & 0x7FF));
		memcpy(packet->data + packet->length, &id, sizeof(uint16_t));
		packet->length += sizeof(uint16_t);

		packet->length += param_serialize_from_var(params[i]->type, params[i]->size, params[i]->value_set, (char *) packet->data + packet->length);

	}

	/* If there were no parameters to be set */
	if (packet->length == 0) {
		csp_buffer_free(packet);
		return 0;
	}

	//csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_SET, 0, CSP_O_CRC32);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return -1;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -1;
	}

	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		csp_close(conn);
		//printf("No response\n");
		return -1;
	}

	//csp_hex_dump("Response", packet->data, packet->length);

	for (int i = 0; i < count; i++) {
		if ((params[i]->value_set == NULL) || (params[i]->value_pending == 0))
			continue;
		params[i]->value_pending = 2;

		if (params[i]->value_get) {
			memcpy(params[i]->value_get, params[i]->value_set, param_size(params[i]));
		}

		if (verbose)
			param_print(params[i]);

	}

	csp_buffer_free(packet);
	csp_close(conn);

	return 0;
}

int param_copy_single(param_t * param, int count, int verbose, int host) {
	param_t * params[1] = { param };
	return param_copy(params, 1, 0, host);
}

int param_copy(param_t * params[], int count, int verbose, int host) {

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	packet->length = 0;
	for (int i = 0; i < count; i++) {

		if (packet->length + sizeof(uint16_t) + param_size(params[i]) > PARAM_SERVER_MTU) {
			printf("Request cropped: > MTU\n");
			break;
		}

		/* Parameter id */
		uint16_t id = csp_hton16((params[i]->node << 11) | (params[i]->id & 0x7FF));
		memcpy(packet->data + packet->length, &id, sizeof(uint16_t));
		packet->length += sizeof(uint16_t);

		packet->length += param_serialize_from_param(params[i], (char *) packet->data + packet->length);

	}

	/* If there were no parameters to be set */
	if (packet->length == 0) {
		csp_buffer_free(packet);
		return 0;
	}

	csp_hex_dump("copy", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_LOG, 0, CSP_O_CRC32);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return -1;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -1;
	}

	csp_close(conn);

}
