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
#include <param/rparam.h>
#include "param_serializer.h"

static void rparam_get_handler(csp_conn_t * conn, csp_packet_t * packet) {

	uint16_t idx[packet->length / 2];
	for (int i = 0; i < packet->length / 2; i++) {
		idx[i] = csp_ntoh16(packet->data16[i]);
	}

	packet->length = param_serialize_idx(idx, packet->length / 2, (void *) packet->data, PARAM_SERVER_MTU);

	//csp_hex_dump("get handler", packet->data, packet->length);

	if (!csp_send(conn, packet, 0))
		csp_buffer_free(packet);
}

static void rparam_set_handler(csp_conn_t * conn, csp_packet_t * packet)
{
	int count = 0;
	while(count < packet->length) {
		count += param_deserialize_single((char *) packet->data + count);
	}
	csp_buffer_free(packet);
}

static void rparam_list_handler(csp_conn_t * conn, csp_packet_t * packet)
{
	csp_buffer_free(packet);
	packet = NULL;

	/* calculate total size */
	param_t * param;
	int paramcount = 0;
	param_foreach(param) {
		paramcount++;
	}

	int totalsize = paramcount * sizeof(rparam_transfer_t);

	printf("Paramcount %u totalsize %u\n", paramcount, totalsize);

	int count = 0;
	while(1) {

		/* Check if we must send */
		if ((packet != NULL) &&
				((packet->length + sizeof(rparam_transfer_t) > PARAM_SERVER_MTU) ||
				(count == paramcount))) {

			/* Send data */
			if (!csp_send(conn, packet, 0)) {
				csp_buffer_free(packet);
				break;
			}

			packet = NULL;
		}

		if (count == paramcount)
			break;

		param = param_index_to_ptr(count);

		printf("Param %s\n", param->name);

		/* Build transfer type */
		rparam_transfer_t rparam_transfer = {
			.idx = param_ptr_to_index(param),
			.type = param->type,
		};
		strncpy(rparam_transfer.name, param->name, 12);

		/* Get new packet */
		if (packet == NULL) {
			packet = csp_buffer_get(PARAM_SERVER_MTU);
			if (packet == NULL)
				return;
			packet->length = 0;
		}

		/* Copy to packet */
		memcpy(packet->data + packet->length, &rparam_transfer, sizeof(rparam_transfer));
		packet->length += sizeof(rparam_transfer);

		count++;

	}

}

csp_thread_return_t rparam_server_task(void *pvParameters)
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
				rparam_get_handler(conn, packet);
				break;

			case PARAM_PORT_SET:
				rparam_set_handler(conn, packet);
				break;

			case PARAM_PORT_LIST:
				rparam_list_handler(conn, packet);
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

