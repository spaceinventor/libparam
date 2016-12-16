/*
 * param_list.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/arch/csp_malloc.h>
#include <csp/csp_endian.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_server.h>

#include "param_string.h"

/**
 * GNU Linker symbols. These will be autogenerate by GCC when using
 * __attribute__((section("param"))
 */
extern param_t __start_param, __stop_param;

/**
 * The storage size (i.e. how closely two param_t structs are packed in memory)
 * varies from platform to platform (in example on x64 and arm32). This macro
 * defines two param_t structs and saves the storage size in a define.
 */
#ifndef PARAM_STORAGE_SIZE
static const param_t param_size_set[2] __attribute__((aligned(1)));
#define PARAM_STORAGE_SIZE ((intptr_t) &param_size_set[1] - (intptr_t) &param_size_set[0])
#endif

/* Convenient macro to loop over the parameter list */
#define param_foreach(_c) \
	for (_c = &__start_param; \
	     _c < &__stop_param; \
	     _c = (param_t *)(intptr_t)((char *)_c + PARAM_STORAGE_SIZE))

static param_t * list_begin = NULL;
static param_t * list_end = NULL;

int param_list_add(param_t * item) {

	if (param_list_find_id(item->node, item->id) != NULL)
		return -1;

	if (list_begin == NULL) {
		list_begin = item;
	}

	if (list_end != NULL)
		list_end->next = item;

	list_end = item;

	item->next = NULL;

	return 0;

}

param_t * param_list_find_id(int node, int id)
{
	if (node == -1)
		node = PARAM_LIST_LOCAL;
	if (node >= 0x1F)
		node = PARAM_LIST_LOCAL;

	param_t * found = NULL;
	int iterator(param_t * param) {

		if (param->node != node)
			return 1;

		if (param->id == id) {
			found = param;
			return 0;
		}

		return 1;
	}
	param_list_foreach(iterator);

	return found;
}

param_t * param_list_find_name(int node, char * name)
{
	if (node == -1)
		node = PARAM_LIST_LOCAL;
	if (node >= 0x1F)
		node = PARAM_LIST_LOCAL;

	param_t * found = NULL;
	int iterator(param_t * param) {

		if (param->node != node)
			return 1;

		if (strcmp(param->name, name) == 0) {
			found = param;
			return 0;
		}

		return 1;
	}
	param_list_foreach(iterator);

	return found;
}

void param_list_print(char * token) {
	int iterator(param_t * param) {
		param_print(param);
		return 1;
	}
	param_list_foreach(iterator);
}

void param_list_foreach(int (*iterator)(param_t * param)) {
	param_t * param;
	param_foreach(param) {
		if (iterator(param) == 0)
			return;
	}
	param = list_begin;
	while(param != NULL) {
		if (iterator(param) == 0)
			return;
		param = param->next;
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
		rparam_transfer_t * new_param = (void *) packet->data;

		int strlen = packet->length - offsetof(rparam_transfer_t, name);
		int size = param_typesize(new_param->type);
		if (size == -1)
			size = new_param->size;

		struct param_heap_s {
			param_t param;
			char name[strlen+1];
			uint8_t value_get[size];
			uint8_t value_set[size];
		} *param_heap = calloc(sizeof(struct param_heap_s), 1);

		/* Allocate new rparam type */
		param_t * param = &param_heap->param;
		if (param == NULL) {
			csp_buffer_free(packet);
			break;
		}
		param->storage_type = PARAM_STORAGE_REMOTE;
		param->node = csp_ntoh16(new_param->id) >> 11;
		param->id = csp_ntoh16(new_param->id) & 0x7FF;
		param->type = new_param->type;
		param->size = size;
		param->unit = NULL;

		/* Name */
		param->name = param_heap->name;
		strncpy(param->name, new_param->name, strlen);
		param->name[strlen] = '\0';

		/* Storage */
		param->value_get = param_heap->value_get;
		param->value_set = param_heap->value_set;

		printf("Got param: %s size (%u)\n", param->name, param->size);

		/* Add to list */
		if (param_list_add(param) != 0)
			free(param);

		csp_buffer_free(packet);
		count++;
	}

	printf("Received %u parameters\n", count);
	csp_close(conn);
}
