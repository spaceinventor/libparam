/*
 * vmem_client.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/arch/csp_malloc.h>
#include <csp/csp_endian.h>

#include "vmem_client.h"

void vmem_download(int node, int timeout, uint64_t address, char * dataout, size_t len)
{
	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_RDP);
	if (conn == NULL)
		return;

	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {
		csp_hex_dump("Response", packet->data, packet->length);
		csp_buffer_free(packet);
	}

	printf("No more data\n");
	csp_close(conn);
}

void vmem_upload(int node, int timeout, uint64_t address, char * datain, size_t len)
{
	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_RDP);
	if (conn == NULL)
		return;

	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {
		csp_hex_dump("Response", packet->data, packet->length);
		csp_buffer_free(packet);
	}

	printf("No more data\n");
	csp_close(conn);
}
