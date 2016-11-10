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

rparam_t * list_begin = NULL;
rparam_t * list_end = NULL;

int rparam_list_add(rparam_t * item) {

	if (rparam_list_find_idx(item->node, item->idx) != NULL)
		return -1;

	if (list_begin == NULL)
		list_begin = item;

	if (list_end != NULL)
		list_end->next = item;

	list_end = item;
	item->next = NULL;

	return 0;

}

rparam_t * rparam_list_find_idx(int node, int idx)
{
	rparam_t * rparam = list_begin;
	while(rparam != NULL) {

		if (rparam->node != node)
			goto next;

		if (rparam->idx != idx)
			goto next;

		return rparam;

next:
		rparam = rparam->next;
	}
	return NULL;
}

rparam_t * rparam_list_find_name(int node, char * name)
{
	rparam_t * rparam = list_begin;
	while(rparam != NULL) {

		if (rparam->node != node)
			goto next;

		if (strcmp(rparam->name, name) != 0)
			goto next;

		return rparam;

next:
		rparam = rparam->next;
	}
	return NULL;
}

void rparam_list_print(int node_filter, int pending) {

	rparam_t * rparam = list_begin;
	while(rparam != NULL) {
		if ((node_filter >= 0) && (rparam->node != node_filter)) {
			rparam = rparam->next;
			continue;
		}
		if (pending && rparam->setvalue_pending != 1) {
			rparam = rparam->next;
			continue;
		}
		rparam_print(rparam);
		rparam = rparam->next;
	}

}

void rparam_list_foreach(void (*iterator)(rparam_t * rparam)) {
	rparam_t * rparam = list_begin;
	while(rparam != NULL) {
		iterator(rparam);
		rparam = rparam->next;
	}
}

void rparam_list_download(int node, int timeout) {

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_LIST, timeout, CSP_O_RDP);
	if (conn == NULL)
		return;

	int count = 0;
	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {

		//csp_hex_dump("Response", packet->data, packet->length);
		rparam_transfer_t * new_param = (void *) packet->data;

		/* Allocate new rparam type */
		rparam_t * rparam = calloc(sizeof(rparam_t), 1);
		rparam->node = node;
		rparam->timeout = timeout;
		rparam->idx = csp_ntoh16(new_param->idx);
		rparam->type = new_param->type;
		rparam->size = new_param->size;

		int strlen = packet->length - offsetof(rparam_transfer_t, name);
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


