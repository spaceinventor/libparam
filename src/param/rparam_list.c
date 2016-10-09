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

typedef struct rparam_list_s {
	rparam_t *rparam;
	struct rparam_list_s *next;
} rparam_list_t;

void rparam_list_add(rparam_t * rparam) {

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

	csp_hex_dump("rparam list", data, datasize);
	csp_free(data);
}


