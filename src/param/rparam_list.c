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
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_LIST, 0, CSP_O_NONE);

	csp_packet_t * packet = csp_buffer_get(1);
	packet->length = 0;
	if (!csp_send(conn, packet, 0)) {
		csp_buffer_free(packet);
		csp_close(conn);
	}

	void * data = NULL;
	int datasize;
	csp_sfp_recv(conn, &data, &datasize, timeout);

	csp_close(conn);

	printf("Received %u bytes\n", datasize);

	if (data == NULL)
		return;

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
}


