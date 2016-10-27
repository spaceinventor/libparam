/*
 * vmem_server.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <csp/csp_endian.h>
#include <csp/arch/csp_thread.h>

#include <vmem/vmem_server.h>

#define VMEM_SERVER_TIMEOUT 30000
#define VMEM_SERVER_MTU 200

#define MAX(a,b) ((a) > (b) ? a : b)
#define MIN(a,b) ((a) < (b) ? a : b)

void vmem_server_handler(csp_conn_t * conn)
{
	/* Read request */
	csp_packet_t * packet = csp_read(conn, VMEM_SERVER_TIMEOUT);
	if (packet == NULL)
		return;

	/* Copy data from request */
	vmem_request_t * request = (void *) packet->data;
	int type = request->type;
	uint32_t address = csp_ntoh32(request->address);
	uint32_t length = csp_ntoh32(request->length);
	csp_buffer_free(packet);

	printf("Type %u, addr 0x%X, length %u\n", type, address, length);

	/**
	 * DOWNLOAD
	 */
	if (type == VMEM_SERVER_DOWNLOAD) {

		printf("Download\n");
		int count = 0;
		while(count < length) {
			printf("Send chunk %u\n", count);
			csp_packet_t * packet = csp_buffer_get(VMEM_SERVER_MTU);
			packet->length = MIN(VMEM_SERVER_MTU, length - count);
			count += packet->length;
			if (!csp_send(conn, packet, VMEM_SERVER_TIMEOUT)) {
				csp_buffer_free(packet);
				return;
			}
		}

	/**
	 * UPLOAD
	 */
	} else if (request->type == VMEM_SERVER_UPLOAD) {

		printf("Upload\n");

	}

}

csp_thread_return_t vmem_server_task(void *pvParameters)
{

	/* Create socket without any socket options */
	csp_socket_t *sock = csp_socket(CSP_SO_NONE);

	/* Bind all ports to socket */
	csp_bind(sock, VMEM_PORT_SERVER);

	/* Create 10 connections backlog queue */
	csp_listen(sock, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;

	/* Process incoming connections */
	while (1) {

		/* Wait for connection, 10000 ms timeout */
		if ((conn = csp_accept(sock, CSP_MAX_DELAY)) == NULL)
			continue;

		/* Handle RDP service differently */
		if (csp_conn_dport(conn) == VMEM_PORT_SERVER) {
			vmem_server_handler(conn);
			csp_close(conn);
			continue;
		}

		/* Close current connection, and handle next */
		csp_close(conn);

	}

	return CSP_TASK_RETURN;

}

