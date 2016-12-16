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
		response->length += param_serialize_single(param, (char *) response->data + response->length, PARAM_SERVER_MTU - response->length);
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

	int count = 0;
	while(count < packet->length) {
		int ret = param_deserialize_single((char *) packet->data + count);
		if (ret == 0) {
			csp_buffer_free(packet);
			return;
		}
		count += ret;
	}

	/* Send ack */
	memcpy(packet->data, "ok", 2);
	packet->length = 2;

	//csp_hex_dump("set handler", packet->data, packet->length);

	if (!csp_send(conn, packet, 0))
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

