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

#include <vmem/vmem.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_fram_secure.h>

void vmem_server_handler(csp_conn_t * conn)
{
	/* Read request */
	csp_packet_t * packet = csp_read(conn, VMEM_SERVER_TIMEOUT);
	if (packet == NULL)
		return;

	/* Copy data from request */
	vmem_request_t * request = (void *) packet->data;
	int type = request->type;

	/**
	 * DOWNLOAD
	 */
	if (type == VMEM_SERVER_DOWNLOAD) {

		uint32_t address = csp_ntoh32(request->data.address);
		uint32_t length = csp_ntoh32(request->data.length);
		csp_buffer_free(packet);

		unsigned int count = 0;
		while(count < length) {

			/* Prepare packet */
			csp_packet_t * packet = csp_buffer_get(VMEM_SERVER_MTU);
			packet->length = VMEM_MIN(VMEM_SERVER_MTU, length - count);

			/* Get data */
			vmem_memcpy(packet->data, (void *) ((intptr_t) address + count), packet->length);

			/* Increment */
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

		uint32_t address = csp_ntoh32(request->data.address);
		csp_buffer_free(packet);

		int count = 0;
		while((packet = csp_read(conn, VMEM_SERVER_TIMEOUT)) != NULL) {

			//csp_hex_dump("Upload", packet->data, packet->length);

			/* Put data */
			vmem_memcpy((void *) ((intptr_t) address + count), packet->data, packet->length);

			/* Increment */
			count += packet->length;

			csp_buffer_free(packet);
		}

	} else if (request->type == VMEM_SERVER_LIST) {

		vmem_list_t * list = (vmem_list_t *) packet->data;

		int i = 0;
		packet->length = 0;
		for(vmem_t * vmem = (vmem_t *) &__start_vmem; vmem < (vmem_t *) &__stop_vmem; vmem++, i++) {
			list[i].vaddr = csp_hton32((intptr_t) vmem->vaddr);
			list[i].size = csp_hton32(vmem->size);
			list[i].vmem_id = i;
			list[i].type = vmem->type;
			strncpy(list[i].name, vmem->name, 8);
			packet->length += sizeof(vmem_list_t);
		}

		if (!csp_send(conn, packet, VMEM_SERVER_TIMEOUT)) {
			csp_buffer_free(packet);
			return;
		}

	} else if ((request->type == VMEM_SERVER_RESTORE) || (request->type == VMEM_SERVER_BACKUP)) {

		int result;
		if (request->type == VMEM_SERVER_BACKUP) {
			result = vmem_fram_secure_backup(vmem_index_to_ptr(request->vmem.vmem_id));
		} else {
			result = vmem_fram_secure_restore(vmem_index_to_ptr(request->vmem.vmem_id));
		}

		packet->data[0] = (int8_t) result;
		packet->length = 1;

		if (!csp_send(conn, packet, VMEM_SERVER_TIMEOUT)) {
			csp_buffer_free(packet);
			return;
		}

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

