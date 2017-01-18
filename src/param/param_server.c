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

static void param_get_handler(csp_conn_t * conn, csp_packet_t * packet) {

	//csp_hex_dump("get handler", packet->data, packet->length);

	/* Get a new response packet */
	csp_packet_t * response = csp_buffer_get(PARAM_SERVER_MTU);
	if (response == NULL) {
		csp_buffer_free(packet);
		return;
	}
	response->length = 0;

	/* Loop through parameters */
	for (int i = 0; i < packet->length / 2; i++) {

		/* convert and find parameter */
		uint16_t id = csp_ntoh16(packet->data16[i]);
		int node = id >> 11;
		if (node == csp_get_address())
			node = PARAM_LIST_LOCAL;
		param_t * param = param_list_find_id(node, id & 0x7FF);
		if (param == NULL)
			continue;

		/* Serialize into response */
		node = param->node;
		if (node == PARAM_LIST_LOCAL)
			node = csp_get_address();
		id = csp_hton16((node << 11) | (param->id & 0x7FF));
		memcpy((char *) response->data + response->length, &id, sizeof(uint16_t));
		response->length += sizeof(uint16_t);

		char tmp[param_size(param)];
		param_get(param, tmp);
		response->length += param_serialize_from_var(param->type, param_size(param), tmp, (char *) response->data + response->length);

		if (response->length >= PARAM_SERVER_MTU)
			break;
	}

	/* Now free the request */
	csp_buffer_free(packet);

	//csp_hex_dump("get handler", response->data, response->length);

	if (!csp_send(conn, response, 0))
		csp_buffer_free(response);
}

static void param_set_handler(csp_conn_t * conn, csp_packet_t * packet)
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

static void param_log_handler(csp_conn_t * conn, csp_packet_t * packet) {

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
			switch (csp_conn_dport(conn)) {

			case PARAM_PORT_GET:
				param_get_handler(conn, packet);
				break;

			case PARAM_PORT_SET:
				param_set_handler(conn, packet);
				break;

			case PARAM_PORT_LOG:
				param_log_handler(conn, packet);
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

