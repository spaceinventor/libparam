/*
 * vmem_server.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <csp/csp.h>
#include <sys/types.h>
#include <csp/arch/csp_time.h>

#include <vmem/vmem.h>
#include <vmem/vmem_server.h>

#include <param/param_list.h>
#include "../param/list/param_list.h"

#include <libparam.h>
#include <param/param_server.h>

static int unlocked = 0;

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

		uint32_t address = be32toh(request->data.address);
		uint32_t length = be32toh(request->data.length);
		csp_buffer_free(packet);

		unsigned int count = 0;
		while(count < length) {

			/* Prepare packet */
			csp_packet_t * packet = csp_buffer_get(VMEM_SERVER_MTU);
			if (packet == NULL) {
				break;
			}
			packet->length = VMEM_MIN(VMEM_SERVER_MTU, length - count);

			/* Get data */
			vmem_memcpy(packet->data, (void *) ((intptr_t) address + count), packet->length);

			/* Increment */
			count += packet->length;

			csp_send(conn, packet);

		}

	/**
	 * UPLOAD
	 */
	} else if (request->type == VMEM_SERVER_UPLOAD) {

		uint32_t address = be32toh(request->data.address);
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
			list[i].vaddr = htobe32((intptr_t) vmem->vaddr);
			list[i].size = htobe32(vmem->size);
			list[i].vmem_id = i;
			list[i].type = vmem->type;
			strncpy(list[i].name, vmem->name, 5);
			packet->length += sizeof(vmem_list_t);
		}

		csp_send(conn, packet);

	} else if ((request->type == VMEM_SERVER_RESTORE) || (request->type == VMEM_SERVER_BACKUP)) {

		vmem_t * vmem = vmem_index_to_ptr(request->vmem.vmem_id);
		int result;
		if (request->type == VMEM_SERVER_BACKUP) {
			if (unlocked == 1 && vmem->backup != NULL) {
				result = vmem->backup(vmem);
			} else {
				result = -4;
			}
		} else {
			if (vmem->restore != NULL) {
				result = vmem->restore(vmem);
			} else {
				result = -3;
			}
		}

		packet->data[0] = (int8_t) result;
		packet->length = 1;

		csp_send(conn, packet);

	} else if (request->type == VMEM_SERVER_UNLOCK) {

		/* Step 1: Check initial unlock code */
		if (be32toh(request->unlock.code) != 0x28140360) {
			csp_buffer_free(packet);
			return;
		}

		/* Step 2: Generate verification sequence */
		unsigned int seed = csp_get_ms();
		uint32_t verification_sequence = (uint32_t) rand_r(&seed);
		request->unlock.code = htobe32(verification_sequence);

		csp_send(conn, packet);

		/* Step 3: Wait for verification return (you have 30 seconds only) */
		if ((packet = csp_read(conn, 30000)) == NULL) {
			return;
		}

		/* Update request pointer */
		request = (void *) packet->data;

		/* Step 4: Validate verification sequence */
		if (be32toh(request->unlock.code) == verification_sequence) {
			unlocked = 1;
			request->unlock.code = htobe32(0);
		} else {
			unlocked = 0;
			request->unlock.code = htobe32(0xFFFFFFFF);
		}

		csp_send(conn, packet);

	}

}

#define MIN(a,b) (((a)<(b))?(a):(b))

static void rparam_list_handler(csp_conn_t * conn)
{
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		csp_packet_t * packet = csp_buffer_get(256);
		if (packet == NULL)
		    break;
		param_transfer2_t * rparam = (void *) packet->data;
		int node = param->node;
		if (node == PARAM_LIST_LOCAL)
			node = csp_get_address();
		rparam->id = htobe16(param->id);
		rparam->node = htobe16(node);
		rparam->type = param->type;
		rparam->size = param->array_size;
		rparam->mask = htobe32(param->mask);
		strncpy(rparam->name, param->name, 35);
		packet->length = offsetof(param_transfer2_t, name) + MIN(strlen(param->name), 35);
		
		csp_send(conn, packet);
	}
}

void vmem_server_loop(void * param) {

	/* Statically allocate a listener socket */
	static csp_socket_t vmem_server_socket = {0};

	/* Bind all ports to socket */
	csp_bind(&vmem_server_socket, VMEM_PORT_SERVER);
	csp_bind(&vmem_server_socket, PARAM_PORT_LIST);

	/* Create 10 connections backlog queue */
	csp_listen(&vmem_server_socket, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;

	/* Process incoming connections */
	while (1) {

		/* Wait for connection, 10000 ms timeout */
		if ((conn = csp_accept(&vmem_server_socket, CSP_MAX_DELAY)) == NULL)
			continue;

		/* Handle RDP service differently */
		if (csp_conn_dport(conn) == VMEM_PORT_SERVER) {
			vmem_server_handler(conn);
			csp_close(conn);
			continue;
		}

		/* Handle RDP service differently */
		if (csp_conn_dport(conn) == PARAM_PORT_LIST) {
			rparam_list_handler(conn);
			csp_close(conn);
			continue;
		}

		/* Close current connection, and handle next */
		csp_close(conn);

	}

}

