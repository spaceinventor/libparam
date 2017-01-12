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
	int iterator_list_find(param_t * param) {

		if (param->node != node)
			return 1;

		if (param->id == id) {
			found = param;
			return 0;
		}

		return 1;
	}
	param_list_foreach(iterator_list_find);

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

		int node = csp_ntoh16(new_param->id) >> 11;
		int id = csp_ntoh16(new_param->id) & 0x7FF;
		int type = new_param->type;

		param_t * param = param_list_create_remote(id, node, type, 0, size, new_param->name, strlen);
		if (param == NULL) {
			csp_buffer_free(packet);
			break;
		}

		printf("Got param: %s size (%u)\n", param->name, param->size);

		/* Add to list */
		if (param_list_add(param) != 0)
			param_list_free(param);

		csp_buffer_free(packet);
		count++;
	}

	printf("Received %u parameters\n", count);
	csp_close(conn);
}

void param_list_free(param_t * param) {
	free(param);
}

param_t * param_list_create_remote(int id, int node, int type, int refresh, int size, char * name, int namelen) {

	struct param_heap_s {
		param_t param;
		char name[namelen+1];
		uint8_t value_get[size];
		uint8_t value_set[size];
	} *param_heap = calloc(sizeof(struct param_heap_s), 1);

	param_t * param = &param_heap->param;
	if (param == NULL) {
		return NULL;
	}

	param->storage_type = PARAM_STORAGE_REMOTE;
	param->name = param_heap->name;
	param->value_get = param_heap->value_get;
	param->value_set = param_heap->value_set;
	param->unit = NULL;

	param->id = id;
	param->node = node;
	param->type = type;
	param->refresh = refresh;
	param->size = size;

	strncpy(param->name, name, namelen);
	param->name[namelen] = '\0';

	//printf("Created %s\n", param->name);

	return param;

}

void param_list_from_string(FILE *stream, int node_override) {

	char line[100];
	char name[25];
	int id, node, type, refresh, size;
	while(fgets(line, 100, stream) != NULL) {

		size = -1;
		refresh = 0;

		int scanned = sscanf(line, "%25[^|]|%u:%u?%u#%u[%d]%*s", name, &id, &node, &type, &refresh, &size);
		//printf("Scanned %u => %s", scanned, line);
		if (scanned == EOF)
			break;

		if (scanned < 4)
			continue;

		if (node_override >= 0)
			node = node_override;

		if (size == -1) {
			size = param_typesize(type);
		}

		param_t * param = param_list_create_remote(id, node, type, refresh, size, name, strlen(name));
		if (param) {
			if (param_list_add(param) < 0) {
				param_list_free(param);
			}
		}
	}
}

void param_list_to_string(FILE * stream, int node_filter, int remote_only) {
	int add_rparam(param_t * param) {
		if ((node_filter >= 0) && (param->node != node_filter))
			return 1;

		if ((remote_only) && (param->storage_type != PARAM_STORAGE_REMOTE))
			return 1;

		int node = param->node;
		if (node == PARAM_LIST_LOCAL)
			node = csp_get_address();

		fprintf(stream, "%s|%u:%u?%u#%u[%d]\n", param->name, param->id, node, param->type, param->refresh, param->size);
		return 1;
	}

	param_list_foreach(add_rparam);
}
