/*
 * param_server.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/csp_endian.h>
#include <param/param.h>
#include <param/param_server.h>
#include <param/param_list.h>

#include "param_log.h"
#include "param_serializer.h"

static void param_pull_request(csp_conn_t * conn, csp_packet_t * request) {

	csp_hex_dump("get handler", request->data, request->length);

	/* Get a new response packet */
	csp_packet_t * response = csp_buffer_get(PARAM_SERVER_MTU);
	if (response == NULL) {
		csp_buffer_free(request);
		return;
	}
	response->length = 0;
	response->data[0] = PARAM_PULL_RESPONSE;
	response->data[1] = 0;
	uint8_t * output = &response->data[2];

	uint32_t timestamp = 0;
	uint8_t node = 255;

	uint8_t * input = &request->data[2];

	printf("Request length %u\n", request->length);
	while(input < request->data + request->length) {

		printf("Input offset %u\n", (unsigned int) (input - request->data));
		switch(*input) {
		case PARAM_CHUNK_TIME:
			input += param_deserialize_chunk_timestamp(&timestamp, input);
			printf("Got timestamp %u\n", (unsigned int) timestamp);
			break;
		case PARAM_CHUNK_NODE:
			input += param_deserialize_chunk_node(&node, input);
			if (node == csp_get_address())
				node = PARAM_LIST_LOCAL;
			printf("Got node %u\n", (unsigned int) node);
			break;
		case PARAM_CHUNK_PARAMS: {
			uint8_t count;
			input += param_deserialize_chunk_params(&count, input);
			printf("Number of paramids %u\n", count);

			uint8_t found_count = 0;
			param_t * found_params[256];

			for (int i = 0; i < count; i++) {
				uint16_t paramid;
				input += param_deserialize_chunk_params_next(&paramid, input);
				printf("Got paramid %u\n", paramid);
				param_t * param = param_list_find_id(node, paramid);
				if (param == NULL)
					continue;
				printf("Found param %s\n", param->name);
				found_params[found_count++] = param;
			}

			output += param_serialize_chunk_param_and_value(found_params, found_count, output);

			break;
		}
		default:
			printf("Invalid type %u\n", *input);
			csp_buffer_free(request);
			csp_buffer_free(response);
			return;
		}

	}

	response->length = output - response->data;

	/* Now free the request */
	csp_buffer_free(request);

	csp_hex_dump("get handler", response->data, response->length);

	if (!csp_send(conn, response, 0))
		csp_buffer_free(response);
}

static void param_push_request(csp_conn_t * conn, csp_packet_t * packet)
{

	//csp_hex_dump("set handler", packet->data, packet->length);

	/* Vairable fields */
	int count = 0;
	while(count < packet->length) {

		uint16_t id;
		memcpy(&id, &packet->data[count], sizeof(id));
		count += sizeof(id);
		id = csp_ntoh16(id);

		int node = id >> 11;
		if (node == csp_get_address())
			node = PARAM_LIST_LOCAL;

		param_t * param = param_list_find_id(node, id & 0x7FF);
		if (param == NULL) {
			csp_buffer_free(packet);
			return;
		}

		char tmp[param_size(param)];
		count += param_deserialize_to_var(param->type, param->size, &packet->data[count], tmp);
		param_set(param, tmp);

	}

	/* Send ack */
	memcpy(packet->data, "ok", 2);
	packet->length = 2;

	//csp_hex_dump("set handler", packet->data, packet->length);

	if (!csp_send(conn, packet, 0))
		csp_buffer_free(packet);

}

static void param_pull_response(csp_conn_t * conn, csp_packet_t * packet) {

	csp_hex_dump("log handler", packet->data, packet->length);

	int i = 0;
	while(i < packet->length) {

		/* Node */
		uint8_t node = packet->data[i++];

		/* Timestamp */
		uint32_t timestamp;
		memcpy(&timestamp, &packet->data[i], sizeof(timestamp));
		i += sizeof(timestamp);
		timestamp = csp_ntoh32(timestamp);

		/* Number of parameters (with that node and timestamp) */
		uint8_t count = packet->data[i++];

		/* Loop over parameters */
		int j = 0;
		while(j < count) {

			uint16_t id;
			memcpy(&id, &packet->data[i], sizeof(id));
			i += sizeof(id);
			id = csp_ntoh16(id);

			param_t * param = param_list_find_id(node, id & 0x7FF);
			if ((param == NULL) || (param->storage_type != PARAM_STORAGE_REMOTE) || (param->value_get == NULL)) {
				/** TODO: Possibly use segment length, instead of parameter count, making it possible to skip a segment */
				printf("Invalid param \n");
				csp_buffer_free(packet);
				return;
			}


			i += param_deserialize_to_var(param->type, param->size, &packet->data[i], param->value_get);

			/**
			 * TODO: value updated is used by collector (refresh interval)
			 * So maybe it should not be used for logging, because the remote timestamp could be invalid
			 * The timestamp could be used for the param_log call instead.
			 */
			param->value_updated = timestamp;
			if (param->value_pending == 2)
				param->value_pending = 0;

			/**
			 * TODO: Param log assumes ordered input
			 */
			param_log(param, param->value_get, timestamp);

		}

	}

	csp_buffer_free(packet);

}

csp_thread_return_t param_server_task(void *pvParameters)
{

	/* Create socket without any socket options */
	csp_socket_t *sock = csp_socket(CSP_SO_NONE);

	/* Bind all ports to socket */
	csp_bind(sock, CSP_ANY);

	/* Create 10 connections backlog queue */
	csp_listen(sock, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;
	csp_packet_t *packet;

	/* Process incoming connections */
	while (1) {

		/* Wait for connection, 10000 ms timeout */
		if ((conn = csp_accept(sock, CSP_MAX_DELAY)) == NULL)
			continue;

		/* Read packets. Timout is 100 ms */
		while ((packet = csp_read(conn, 0)) != NULL) {
			switch (packet->data[0]) {

			case PARAM_PULL_REQUEST:
				param_pull_request(conn, packet);
				break;

			case PARAM_PULL_RESPONSE:
				param_pull_response(conn, packet);
				break;

			case PARAM_PUSH_REQUEST:
				param_push_request(conn, packet);
				break;

			default:
				/* Let the service handler reply pings, buffer use, etc. */
				csp_service_handler(conn, packet);
				break;
			}
		}

		/* Close current connection, and handle next */
		csp_close(conn);

	}

	return CSP_TASK_RETURN;

}

