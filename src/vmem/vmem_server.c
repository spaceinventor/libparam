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
#include <csp/csp_crc32.h>

#include <vmem/vmem.h>
#include <vmem/vmem_crc32.h>
#include <vmem/vmem_server.h>

#include <param/param_list.h>
#include "../param/list/param_list.h"
#include <vmem/vmem_compress.h>
#include "vmem_internal.h"

#include <libparam.h>
#include <param/param_server.h>

SLIST_HEAD(vmem_handler_obj_list_s, vmem_handler_obj_s);
typedef struct vmem_handler_obj_list_s vmem_handler_obj_list_t;

static int unlocked = 0;
static vmem_handler_obj_list_t g_vmem_handler_list = SLIST_HEAD_INITIALIZER();

static vmem_handler_obj_t *vmem_server_find_handler(uint8_t type) {

	vmem_handler_obj_t *iter;
	iter = SLIST_FIRST(&g_vmem_handler_list);
	while (iter) {
		if (iter->type == type) {
			return iter;
		}
		iter = SLIST_NEXT(iter, list);
	}
	return NULL;
}

int vmem_server_bind_type(uint8_t type, vmem_handler_t *func, vmem_handler_obj_t *obj, void *context) {

	if (type < VMEM_SERVER_USER_TYPES) {
		printf("warn: vmem_server_bind_type: type must have bit-7 set to indicate user type\n");
		return -1;
	}

	/* Check if the type is already bound */
	vmem_handler_obj_t *iter;
	iter = SLIST_FIRST(&g_vmem_handler_list);
	while (iter) {
		if (iter->type == type) {
			printf("warn: vmem_server_bind_type: type (%"PRIu8") already bound\n", type);
			return -1;
		}
		iter = SLIST_NEXT(iter, list);
	}

	/* Insert the handler object into the list of handler objects */
	obj->type = type;
	obj->handler = func;
	obj->context = context;
	SLIST_INSERT_HEAD(&g_vmem_handler_list, obj, list);

	return 0;
}

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
	if (type == VMEM_SERVER_DOWNLOAD || type == VMEM_SERVER_CALCULATE_CRC32) {

		uint64_t length;
		uint64_t address;
		
		if (request->version == 3) {
			address = be64toh(request->data3.address);
			length = be64toh(request->data3.length);
		} else if (request->version == 2) {
			address = be64toh(request->data2.address);
			length = be32toh(request->data2.length);
		} else {
			address = be32toh(request->data.address);
			length = be32toh(request->data.length);
		}

		//printf("Download from:");
		//printf("  Addr %"PRIx64"\n", address);
		//printf("  Length %"PRIu32"\n", length);

		uint64_t count = 0;
		if (type == VMEM_SERVER_DOWNLOAD) {
			/* We have to free the requesting packet, since we are going to
			 * allocate a bunch of them for the reply.
			 */
			csp_buffer_free(packet);

			while((count < length) && csp_conn_is_active(conn)) {
				/* Prepare packet */
				csp_packet_t * packet = csp_buffer_get(VMEM_SERVER_MTU);
				if (packet == NULL) {
					break;
				}
				packet->length = VMEM_MIN(VMEM_SERVER_MTU, length - count);

				/* Get data */
				vmem_read(packet->data, address + count, packet->length);

				/* Increment */
				count += packet->length;

				csp_send(conn, packet);
			}
		} else if (type == VMEM_SERVER_CALCULATE_CRC32) {

			/* Do the CRC32 calculation on the address area (vmem) using the request packet as the buffer */
			uint32_t crc = vmem_calc_crc32(address, length, &packet->data[0], VMEM_SERVER_MTU);

			/* Convert to network byte order */
			crc = htobe32(crc);

			/* Copy checksum to packet */
			memcpy(&packet->data[0], &crc, sizeof(crc));
			packet->length = sizeof(crc);

			/* Send the response */
			csp_send(conn, packet);
		}

	/**
	 * UPLOAD
	 */
	} else if (request->type == VMEM_SERVER_UPLOAD) {

		uint64_t address;
		
		if (request->version == 2) {
			address = be64toh(request->data2.address);
		} else {
			address = be32toh(request->data.address);
		}
		csp_buffer_free(packet);

		uint32_t count = 0;
		while((packet = csp_read(conn, VMEM_SERVER_TIMEOUT)) != NULL) {

			//csp_hex_dump("Upload", packet->data, packet->length);

			/* Put data */
			vmem_write(address + count, packet->data, packet->length);

			/* Increment */
			count += packet->length;

			csp_buffer_free(packet);
		}
		// Now flush the VMEM cache associated with the address, if any
		vmem_t *vmem = vmem_vaddr_to_vmem(address);
		(void)vmem_flush(vmem);

	} else if (request->type == VMEM_SERVER_LIST) {

		if (request->version == 1) {

			vmem_list_t * list = (vmem_list_t *) packet->data;

			int i = 0;
			packet->length = 0;
			vmem_t *vmem;
			for (vmem_iter_t *iter = vmem_next(NULL); iter != NULL; iter = vmem_next(iter), i++) {
				vmem = vmem_from_iter(iter);
				list[i].vaddr = htobe32((uint32_t)(vmem->vaddr & 0x00000000FFFFFFFFULL));
				list[i].size = htobe32((uint32_t)(vmem->size & 0x00000000FFFFFFFFULL));
				list[i].vmem_id = i;
				list[i].type = vmem->type;
				strncpy(list[i].name, vmem->name, 5);
				packet->length += sizeof(vmem_list_t);
			}

			csp_send(conn, packet);

		} else if (request->version == 2) {
			
			vmem_list2_t * list = (vmem_list2_t *) packet->data;

			int i = 0;
			packet->length = 0;
			vmem_t *vmem;
			for (vmem_iter_t *iter = vmem_next(NULL); iter != NULL; iter = vmem_next(iter), i++) {
				vmem = vmem_from_iter(iter);
				list[i].vaddr = htobe64(vmem->vaddr);
				list[i].size = htobe32((uint32_t)(vmem->size & 0x00000000FFFFFFFFULL));
				list[i].vmem_id = i;
				list[i].type = vmem->type;
				strncpy(list[i].name, vmem->name, 5);
				packet->length += sizeof(vmem_list2_t);
			}

			csp_send(conn, packet);

		} else if (request->version == 3) {

			vmem_list3_t * list;

			int i = 0;

			/* The first byte of each packet contains the flag signalling the first and last packet */
			packet->length = 1;
			packet->data[0] = 0b01000000; /* First packet */
			list = (vmem_list3_t *)&packet->data[packet->length];

			vmem_t *vmem;
			for (vmem_iter_t *iter = vmem_next(NULL); iter != NULL; iter = vmem_next(iter)) {
				vmem = vmem_from_iter(iter);
				if ((packet->length + sizeof(vmem_list3_t)) > VMEM_SERVER_MTU) {
					/* We need to advance to the next packet, but first send the existing one */
					csp_send(conn, packet);
					packet = csp_buffer_get(VMEM_SERVER_MTU);
					if (!packet) {
						printf("Error allocating CSP packet for VMEM list response.\n");
						break;
					}
					packet->length = 1;
					packet->data[0] = 0b00000000;
					list = (vmem_list3_t *)&packet->data[packet->length];
				}

				/* Fill in the VMEM data, handle cases when name does not contain NULL termination */
				strncpy(list->name, vmem->name, sizeof(list->name)-1);
				list->name[sizeof(list->name)-1] = '\0';
				list->vaddr = htobe64(vmem->vaddr);
				list->size = htobe64(vmem->size);
				list->type = vmem->type;
				list->vmem_id = i;
				packet->length += sizeof(vmem_list3_t);

				/* Advance to the next VMEM */
				list++;
			}

			packet->data[0] |= 0b10000000; /* Last packet */
			csp_send(conn, packet);
		}

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

	} else if (request->type == VMEM_SERVER_DECOMPRESS) {
		uint64_t final_len;
		int result = -1;

		vmem_decompress_fnc_t decompress_fnc = vmem_server_get_decompress_fnc();

		if(decompress_fnc == NULL) {
			packet->data[0] = VMEM_SERVER_ENOSYS;
			packet->length = 1;
			csp_send(conn, packet);
			return;
		}

		uint64_t src_addr = be64toh(request->data4.src_address);
		uint64_t src_len = be64toh(request->data4.length);
		uint64_t dst_addr = be64toh(request->data4.dst_address);

		packet->data[0] = decompress_fnc(dst_addr, &final_len, src_addr, src_len);
		packet->length = 1;
		csp_send(conn, packet);
	} else {

		/* Check the list of handlers, if we have one which will handle it */
		vmem_handler_obj_t *obj = vmem_server_find_handler(request->type);
		if (obj) {		
			/* Call the handler */
			(void)(*obj->handler)(conn, packet, obj->context);
		} else {
			/* If no handler was found, free the packet */
			printf("warn: vmem_server_handler: unknown type %"PRIu8"\n", request->type);
			/* Free packet if not valid VMEM service request */
			csp_buffer_free(packet);
		}

	}

}

#define MIN(a,b) (((a)<(b))?(a):(b))

static void rparam_list_handler(csp_conn_t * conn)
{
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		if (param->mask & PM_HIDDEN) {
			continue;
		}
		csp_packet_t * packet = csp_buffer_get(256);
		if (packet == NULL)
			break;

		memset(packet->data, 0, 256);

		param_transfer3_t * rparam = (void *) packet->data;
		int node = *param->node;
		rparam->id = htobe16(param->id);
		rparam->node = htobe16(node);
		rparam->type = param->type;
		rparam->size = param->array_size;
		rparam->mask = htobe32(param->mask);
		
		strncpy(rparam->name, param->name, 35);

		if (param->vmem) {
			rparam->storage_type = param->vmem->type;
		}

		if (param->unit != NULL) {
			strncpy(rparam->unit, param->unit, 9);
		}
		int helplen = 0;
		if (param->docstr != NULL) {
			strncpy(rparam->help, param->docstr, 149);
			helplen = strnlen(param->docstr, 149);
		}
		//packet->length = sizeof(param_transfer3_t);
		packet->length = offsetof(param_transfer3_t, help) + helplen + 1;
		
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
