/*
 * param_list.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include "libparam.h"
#ifdef PARAM_LIST_DYNAMIC
#include <malloc.h>
#endif

#include <csp/csp.h>
#include <sys/types.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_server.h>

#include "../param_string.h"
#include "param_list.h"


#ifdef PARAM_HAVE_SYS_QUEUE
#include <sys/queue.h>
#endif

/**
 * The storage size (i.e. how closely two param_t structs are packed in memory)
 * varies from platform to platform (in example on x64 and arm32). This macro
 * defines two param_t structs and saves the storage size in a define.
 */
#ifndef PARAM_STORAGE_SIZE
static param_t param_size_set[2] __attribute__((aligned(1)));
#define PARAM_STORAGE_SIZE ((intptr_t) &param_size_set[1] - (intptr_t) &param_size_set[0])
#endif

#ifdef PARAM_HAVE_SYS_QUEUE
static SLIST_HEAD(param_list_head_s, param_s) param_list_head = {};
#endif

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
#ifdef PARAM_HAVE_SYS_QUEUE
			iterator->element = SLIST_FIRST(&param_list_head);
#endif
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
#ifdef PARAM_HAVE_SYS_QUEUE
		iterator->element = SLIST_FIRST(&param_list_head);
		return iterator->element;
#else
		return NULL;
#endif
	}

#ifdef PARAM_HAVE_SYS_QUEUE
	/* Dynamic phase */
	if (iterator->phase == 1) {

		iterator->element = SLIST_NEXT(iterator->element, next);
		return iterator->element;
	}
#endif

	return NULL;

}


int param_list_add(param_t * item) {
	if (param_list_find_id(item->node, item->id) != NULL)
		return -1;
#ifdef PARAM_HAVE_SYS_QUEUE
	SLIST_INSERT_HEAD(&param_list_head, item, next);
#else
	return -1;
#endif
	return 0;
}

param_t * param_list_find_id(int node, int id) {
	
	if (node < 0)
		node = 0;

	param_t * found = NULL;
	param_t * param;
	param_list_iterator i = {};

	while ((param = param_list_iterate(&i)) != NULL) {

		if (param->node != node)
			continue;

		if (param->id == id) {
			found = param;
			break;
		}

		continue;
	}

	return found;
}

param_t * param_list_find_name(int node, char * name) {
	
	if (node < 0 )
		node = 0;

	param_t * found = NULL;
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if (param->node != node)
			continue;

		if (strcmp(param->name, name) == 0) {
			found = param;
			break;
		}

		continue;
	}

	return found;
}

void param_list_print(uint32_t mask, int verbosity) {
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		if ((param->mask & mask) || (mask == 0xFFFFFFFF)) {
			param_print(param, -1, NULL, 0, verbosity);
		}
	}
}

void param_list_download(int node, int timeout, int list_version) {

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_LIST, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return;

	int count = 0;
	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {

		//csp_hex_dump("Response", packet->data, packet->length);

		int strlen;
		int addr;
		int id;
		int type;
		int size;
		int mask;
		int storage_type = -1;
		char * name;
		char * unit;
		char * help;

	    if (list_version == 1) {

	        param_transfer_t * new_param = (void *) packet->data;
	        name = new_param->name;
			strlen = packet->length - offsetof(param_transfer_t, name);
			name[strlen] = '\0';
	        addr = be16toh(new_param->id) >> 11;
            id = be16toh(new_param->id) & 0x7FF;
            type = new_param->type;
            size = new_param->size;
            mask = be32toh(new_param->mask) | PM_REMOTE;
			unit = NULL;
			help = NULL;

	    } else if (list_version == 2) {

	        param_transfer2_t * new_param = (void *) packet->data;
	        name = new_param->name;
			strlen = packet->length - offsetof(param_transfer2_t, name);
			name[strlen] = '\0';			
            addr = be16toh(new_param->node);
            id = be16toh(new_param->id) & 0x7FF;
            type = new_param->type;
            size = new_param->size;
            mask = be32toh(new_param->mask) | PM_REMOTE;
			unit = NULL;
			help = NULL;

	    } else {

			param_transfer3_t * new_param = (void *) packet->data;
	        name = new_param->name;
            addr = be16toh(new_param->node);
            id = be16toh(new_param->id) & 0x7FF;
            type = new_param->type;
            size = new_param->size;
            mask = be32toh(new_param->mask) | PM_REMOTE;
			storage_type = new_param->storage_type;
			unit = new_param->unit;
			help = new_param->help;

		}

		if (addr == 0)
			addr = node;

		if (size == 255)
			size = 1;

		//printf("Storage type %d\n", storage_type);

		param_t * param = param_list_create_remote(id, addr, type, mask, size, name, unit, help, storage_type);
		if (param == NULL) {
			csp_buffer_free(packet);
			break;
		}

		printf("Got param: %s:%u[%d]\n", param->name, param->node, param->array_size);

		/* Add to list */
		if (param_list_add(param) != 0)
			param_list_destroy(param);

		csp_buffer_free(packet);
		count++;
	}

	printf("Received %u parameters\n", count);
	csp_close(conn);
}

#ifdef PARAM_LIST_DYNAMIC
void param_list_destroy(param_t * param) {
	free(param);
}

param_t * param_list_create_remote(int id, int node, int type, uint32_t mask, int array_size, char * name, char * unit, char * help, int storage_type) {

	if (array_size < 1)
		array_size = 1;

	struct param_heap_s {
		param_t param;
		union {
			uint64_t alignme;
			uint8_t buffer[param_typesize(type) * array_size];
		};
		char name[36];
		char unit[10];
		char help[150];
	} *param_heap = calloc(sizeof(struct param_heap_s), 1);

	param_t * param = &param_heap->param;
	if (param == NULL) {
		return NULL;
	}

	param->vmem = NULL;
	param->name = param_heap->name;
	param->addr = param_heap->buffer;
	param->unit = param_heap->unit;
	param->docstr = param_heap->help;

	param->id = id;
	param->node = node;
	param->type = type;
	param->mask = mask;
	param->array_size = array_size;
	param->array_step = param_typesize(type);

	strncpy(param->name, name, 36);
	if (unit != NULL)
		strncpy(param->unit, unit, 10);

	switch(storage_type) {
		case 0:
		case 1:
			sprintf(param->docstr, "RAM\t"); break;
		case 2:
		case 3:
			sprintf(param->docstr, "FRAM\t"); break;
		case 8:
			sprintf(param->docstr, "FRAM+C\t"); break;
		case 4:
			sprintf(param->docstr, "FLASH\t"); break;
		case 5:
			sprintf(param->docstr, "DRIVER\t"); break;
		case 6:
			sprintf(param->docstr, "QSPIFL\t"); break;
		case 7:
			sprintf(param->docstr, "FILE\t"); break;
		default:
			sprintf(param->docstr, "%d\t", storage_type); break;
			break;
	}
	if (help != NULL)
		strncat(param->docstr, help, 150);

	return param;

}
#endif

