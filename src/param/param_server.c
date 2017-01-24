/*
 * param_server.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>
#include <param/param.h>
#include <param/param_server.h>
#include <param/param_list.h>

#include "param_log.h"
#include "param_serializer.h"

void param_serve_pull_request(csp_conn_t * conn, csp_packet_t * request) {

	//csp_hex_dump("get handler", request->data, request->length);

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

	while(input < request->data + request->length) {
		switch(*input) {
		case PARAM_CHUNK_TIME:
			input += param_deserialize_chunk_timestamp(&timestamp, input);
			output += param_serialize_chunk_timestamp(timestamp, output);
			break;
		case PARAM_CHUNK_NODE:
			input += param_deserialize_chunk_node(&node, input);
			output += param_serialize_chunk_node(node, output);
			break;
		case PARAM_CHUNK_PARAMS: {
			uint8_t count;
			input += param_deserialize_chunk_params_begin(&count, input);

			uint8_t found_count = 0;
			param_t * found_params[256];

			for (int i = 0; i < count; i++) {
				uint16_t paramid;
				input += param_deserialize_chunk_params_next(&paramid, input);
				param_t * param = param_list_find_id(node, paramid);
				if (param == NULL)
					continue;
				found_params[found_count++] = param;
			}

			output += param_serialize_chunk_param_and_value(found_params, found_count, output, 0);

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

	//csp_hex_dump("get handler", response->data, response->length);

	if (!csp_send(conn, response, 0))
		csp_buffer_free(response);
}

void param_serve_pull_response(csp_conn_t * conn, csp_packet_t * packet, int verbose) {

	//csp_hex_dump("pull response", packet->data, packet->length);

	uint32_t timestamp = csp_get_ms();
	uint8_t node = csp_conn_src(conn);

	uint8_t * input = &packet->data[2];
	while(input < packet->data + packet->length) {
		switch(*input) {
			case PARAM_CHUNK_TIME:
				input += param_deserialize_chunk_timestamp(&timestamp, input);
				break;
			case PARAM_CHUNK_NODE:
				input += param_deserialize_chunk_node(&node, input);
				break;
			case PARAM_CHUNK_PARAM_AND_VALUE: {
				input += param_deserialize_chunk_param_and_value(node, timestamp, verbose, input);
				break;
			}
			default:
				printf("Invalid type %u\n", *input);
				csp_buffer_free(packet);
				return;
		}
	}

	csp_buffer_free(packet);

}

static void param_serve_push(csp_conn_t * conn, csp_packet_t * packet)
{

	//csp_hex_dump("set handler", packet->data, packet->length);

	uint32_t timestamp = csp_get_ms();
	uint8_t node = 255;

	uint8_t * input = &packet->data[2];
	while(input < packet->data + packet->length) {
		switch(*input) {
			case PARAM_CHUNK_TIME:
				input += param_deserialize_chunk_timestamp(&timestamp, input);
				break;
			case PARAM_CHUNK_NODE:
				input += param_deserialize_chunk_node(&node, input);
				break;
			case PARAM_CHUNK_PARAM_AND_VALUE: {
				input += param_deserialize_chunk_param_and_value(node, timestamp, 0, input);
				break;
			}
			default:
				printf("Invalid type %u\n", *input);
				csp_buffer_free(packet);
				return;
		}
	}

	/* Send ack */
	memcpy(packet->data, "ok", 2);
	packet->length = 2;

	//csp_hex_dump("set handler", packet->data, packet->length);

	if (!csp_send(conn, packet, 0))
		csp_buffer_free(packet);

}

static void param_serve(csp_conn_t * conn, csp_packet_t * packet) {
	switch(packet->data[0]) {
		case PARAM_PULL_REQUEST:
			param_serve_pull_request(conn, packet);
			break;

		case PARAM_PULL_RESPONSE:
			param_serve_pull_response(conn, packet, 1);
			break;

		case PARAM_PUSH_REQUEST:
			param_serve_push(conn, packet);
			break;

		default:
			printf("Unknown parameter request\n");
			break;
	}
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
			switch (csp_conn_dport(conn)) {

			case PARAM_PORT_SERVER:
				param_serve(conn, packet);
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

