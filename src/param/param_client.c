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

#define MAX_NODES 10

csp_packet_t * param_pull_request(param_t * params[], int count, int host) {

	if (count <= 0)
		return NULL;

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return NULL;

	/* Generate pull request */
	packet->data[0] = PARAM_PULL_REQUEST;
	packet->data[1] = 0;
	uint8_t * output = &packet->data[2];

	/* State variables for serializer */
	int node = 255;
	uint8_t * count_output = NULL;

	/* Loop over parameters in list:
	 * We assume that the list is ordered/grouped by node
	 */
	for (int i = 0; i < count; i++) {

		/* Check if node and host is the same */
		int node_param = params[i]->node;
		if (node_param == host)
			node_param = 255;

		/* Check if we need to change node */
		if (node != node_param) {
			node = node_param;
			output += param_serialize_chunk_node(params[i]->node, output);
			count_output = NULL;
		}

		/* Start a new params chunk */
		if (count_output == NULL) {
			output += param_serialize_chunk_params_begin(&count_output, output);
		}

		/* Add additional parameters to chunk */
		output += param_serialize_chunk_params_next(params[i], count_output, output);

	}

	/* Calculate frame length */
	packet->length = output - packet->data;

	return packet;
}

int param_pull_single(param_t * param, int host, int timeout) {
	param_t * params[1] = { param };
	return param_pull(params, 1, 0, host, timeout);
}

int param_pull(param_t * params[], int count, int verbose, int host, int timeout) {

	csp_packet_t * packet = param_pull_request(params, count, host);
	if (packet == NULL)
		return -1;

	//csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_SERVER, 0, CSP_O_CRC32);
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

	param_serve_pull_response(conn, packet, verbose);
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

	packet->data[0] = PARAM_PUSH_REQUEST;
	packet->data[1] = 0;
	uint8_t * output = &packet->data[2];
	output += param_serialize_chunk_param_and_value(params, count, output, 1);
	packet->length = output - packet->data;

	/* If there were no parameters to be set */
	if (packet->length == 0) {
		csp_buffer_free(packet);
		return 0;
	}

	//csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_SERVER, 0, CSP_O_CRC32);
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
