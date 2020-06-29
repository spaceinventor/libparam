/*
 * param_list.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <sys/queue.h>
#include <malloc.h>

#include <csp/csp.h>
#include <csp/arch/csp_malloc.h>
#include <csp/csp_endian.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_server.h>

#include "../param_string.h"
#include "param_list.h"

/**
 * The storage size (i.e. how closely two param_t structs are packed in memory)
 * varies from platform to platform (in example on x64 and arm32). This macro
 * defines two param_t structs and saves the storage size in a define.
 */
#ifndef PARAM_STORAGE_SIZE
static const param_t param_size_set[2] __attribute__((aligned(1)));
#define PARAM_STORAGE_SIZE ((intptr_t) &param_size_set[1] - (intptr_t) &param_size_set[0])
#endif

static SLIST_HEAD(param_list_head_s, param_s) param_list_head = {};

param_t * param_list_iterate(param_list_iterator * iterator) {

	/**
	 * GNU Linker symbols. These will be autogenerate by GCC when using
	 * __attribute__((section("param"))
	 */
	__attribute__((weak)) extern param_t __start_param;
	__attribute__((weak)) extern param_t __stop_param;

	/* First element */
	if (iterator->element == NULL) {

		/* Static */
		if ((&__start_param != NULL) && (&__start_param != &__stop_param)) {
			iterator->phase = 0;
			iterator->element = &__start_param;
		} else {
			iterator->phase = 1;
			iterator->element = SLIST_FIRST(&param_list_head);
		}

		return iterator->element;
	}

	/* Static phase */
	if (iterator->phase == 0) {

		/* Increment in static memory */
		iterator->element = (param_t *)(intptr_t)((char *)iterator->element + PARAM_STORAGE_SIZE);

		/* Check if we are still within the bounds of the static memory area */
		if (iterator->element < &__stop_param)
			return iterator->element;

		/* Otherwise, switch to dynamic phase */
		iterator->phase = 1;
		iterator->element = SLIST_FIRST(&param_list_head);
		return iterator->element;
	}

	/* Dynamic phase */
	if (iterator->phase == 1) {

		iterator->element = SLIST_NEXT(iterator->element, next);
		return iterator->element;
	}

	return NULL;

}


int param_list_add(param_t * item) {
	if (param_list_find_id(item->node, item->id) != NULL)
		return -1;
	SLIST_INSERT_HEAD(&param_list_head, item, next);
	return 0;
}

param_t * param_list_find_id(int node, int id)
{
	if (node == -1)
		node = PARAM_LIST_LOCAL;
	if (node == csp_get_address())
		node = PARAM_LIST_LOCAL;

	param_t * found = NULL;
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if (param->node != (uint8_t) node)
			continue;

		if (param->id == id) {
			found = param;
			break;
		}

		continue;
	}

	return found;
}

param_t * param_list_find_name(int node, char * name)
{
	if (node == -1)
		node = PARAM_LIST_LOCAL;

	param_t * found = NULL;
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if (param->node != (uint8_t) node)
			continue;

		if (strcmp(param->name, name) == 0) {
			found = param;
			break;
		}

		continue;
	}

	return found;
}

void param_list_print(uint32_t mask) {
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		if ((param->mask & mask) || (mask == 0xFFFFFFFF)) {
			param_print(param, -1, NULL, 0, 2);
		}
	}
}

void param_list_download(int node, int timeout) {

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_LIST, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return;

	int count = 0;
	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {

		//csp_hex_dump("Response", packet->data, packet->length);
		param_transfer_t * new_param = (void *) packet->data;

		int strlen = packet->length - offsetof(param_transfer_t, name);
		int node = csp_ntoh16(new_param->id) >> 11;
		int id = csp_ntoh16(new_param->id) & 0x7FF;
		int type = new_param->type;
		int size = new_param->size;
		int mask = csp_ntoh32(new_param->mask);
		if (size == 255)
			size = 1;

		param_t * param = param_list_create_remote(id, node, type, mask, size, new_param->name, strlen);
		if (param == NULL) {
			csp_buffer_free(packet);
			break;
		}

		printf("Got param: %s[%d]\n", param->name, param->array_size);

		/* Add to list */
		if (param_list_add(param) != 0)
			param_list_destroy(param);

		csp_buffer_free(packet);
		count++;
	}

	printf("Received %u parameters\n", count);
	csp_close(conn);
}

void param_list_destroy(param_t * param) {
	free(param);
}

param_t * param_list_create_remote(int id, int node, int type, uint32_t mask, int array_size, char * name, int namelen) {

	if (array_size < 1)
		array_size = 1;

	struct param_heap_s {
		param_t param;
		union {
			uint64_t alignme;
			uint8_t buffer[param_typesize(type) * array_size];
		};
		char name[namelen+1];
	} *param_heap = calloc(sizeof(struct param_heap_s), 1);

	param_t * param = &param_heap->param;
	if (param == NULL) {
		return NULL;
	}

	param->vmem = NULL;
	param->name = param_heap->name;
	param->addr = param_heap->buffer;
	param->unit = NULL;

	param->id = id;
	param->node = node;
	param->type = type;
	param->mask = mask;
	param->array_size = array_size;
	param->array_step = param_typesize(type);

	strncpy(param->name, name, namelen);
	param->name[namelen] = '\0';

	//printf("Created %s\n", param->name);

	return param;

}

