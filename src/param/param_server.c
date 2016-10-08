/*
 * param_server.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/csp_endian.h>
#include <param/param.h>
#include <param/rparam.h>
#include "param_serializer.h"

static void param_get_handler(csp_conn_t * conn, csp_packet_t * packet) {
	param_request_t * request = (param_request_t *) packet->data;

	uint16_t idx[1];
	idx[0] = csp_ntoh16(request->id);
	param_serialize_idx(idx, 1, (void *) packet->data, PARAM_SERVER_MTU);
	packet->length = 20;

	if (!csp_send(conn, packet, 0))
		csp_buffer_free(packet);
}

static void param_set_handler(csp_conn_t * conn, csp_packet_t * packet)
{
	param_deserialize_single((char *) packet->data);
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

