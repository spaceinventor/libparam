/*
 * rparam_list.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/arch/csp_malloc.h>
#include <csp/csp_endian.h>
#include <param/param.h>
#include <param/rparam.h>
#include <param/rparam_list.h>

#include "param_string.h"

static param_t * list_begin = NULL;
static param_t * list_end = NULL;

int rparam_list_add(param_t * item) {

	if (rparam_list_find_id(item->node, item->id) != NULL)
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

param_t * rparam_list_find_id(int node, int id)
{
	param_t * param;

	/* First search static memory */
	if (node == 255) {
		param_foreach(param) {
			if (param->id == id)
				return param;
		}
	}

	/* Then serach dynamic memory */
	param = list_begin;
	while(param != NULL) {

		//if (param->node != node)
		//	goto next;

		if (param->id != id)
			goto next;

		return param;

next:
		param = param->next;
	}
	return NULL;

}

param_t * rparam_list_find_name(int node, char * name)
{
	param_t * param;

	/* First seach static memory */
	if (node == 255) {

		param_foreach(param) {
			if (strcmp(param->name, name) == 0) {
				return param;
			}
		}
	}

	/* Then search dynamic memory */
	param = list_begin;
	while(param != NULL) {

		//if (param->node != node)
		//	goto next;

		if (strcmp(param->name, name) != 0)
			goto next;

		return param;

next:
		param = param->next;
	}
	return NULL;
}

void rparam_list_print(char * token) {
	void iterator(param_t * param) {
		param_print(param);
	}
	rparam_list_foreach(iterator);
}

void rparam_list_foreach(void (*iterator)(param_t * rparam)) {
	param_t * param;
	param_foreach(param) {
		iterator(param);
	}
	param = list_begin;
	while(param != NULL) {
		iterator(param);
		param = param->next;
	}
}

void rparam_list_download(int node, int timeout) {

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_LIST, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return;

	int count = 0;
	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {

		//csp_hex_dump("Response", packet->data, packet->length);
		rparam_transfer_t * new_param = (void *) packet->data;

		/* Allocate new rparam type */
		param_t * rparam = calloc(sizeof(param_t), 1);
		if (rparam == NULL) {
			csp_buffer_free(packet);
			break;
		}
		rparam->paramtype = 1;
		rparam->node = node;
		rparam->timeout = timeout;
		rparam->id = csp_ntoh16(new_param->id);
		rparam->type = new_param->type;
		rparam->size = new_param->size;
		rparam->unit = NULL;

		int strlen = packet->length - offsetof(rparam_transfer_t, name);
		rparam->name = malloc(strlen);
		strncpy(rparam->name, new_param->name, strlen);
		rparam->name[strlen] = '\0';

		printf("Got param: %s\n", rparam->name);

		/* Add to list */
		if (rparam_list_add(rparam) != 0)
			free(rparam);

		csp_buffer_free(packet);
		count++;
	}

	printf("Received %u parameters\n", count);
	csp_close(conn);
}
