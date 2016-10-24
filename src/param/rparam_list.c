/*
 * rparam_list.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/arch/csp_malloc.h>
#include <param/param.h>
#include <param/rparam.h>
#include "rparam_list.h"

rparam_t * list_begin = NULL;
rparam_t * list_end = NULL;

void rparam_list_add(rparam_t * item) {

	if (list_begin == NULL)
		list_begin = item;

	if (list_end != NULL)
		list_end->next = item;

	list_end = item;
	item->next = NULL;

}

void rparam_list_find(rparam_t * item) {

}

void rparam_list_foreach(void) {

	rparam_t * rparam = list_begin;
	while(rparam != NULL) {
		printf("list %s\n", rparam->name);
		rparam = rparam->next;
	}

}

void rparam_list_download(int node, int timeout) {

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_LIST, timeout, CSP_O_RDP);
	if (conn == NULL)
		return;

	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {
		csp_hex_dump("Response", packet->data, packet->length);
		csp_buffer_free(packet);
	}

	printf("No more data\n");
	csp_close(conn);

#if 0
	for (rparam_transfer_t * rtrans = data; (intptr_t) rtrans < (intptr_t) data + datasize; rtrans++) {
		printf("Param %s\n", rtrans->name);
		rparam_t * rparam = csp_malloc(sizeof(param_t));
		if (rparam == NULL)
			break;
		rparam->idx = rtrans->idx;
		rparam->type = rtrans->type;
		rparam->node = node;
		rparam->timeout = timeout;
		strncpy(rparam->name, rtrans->name, 13);
		rparam_list_add(rparam);
	}

	csp_hex_dump("rparam list", data, datasize);
	csp_free(data);
#endif
}


