/*
 * vmem_client.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/arch/csp_malloc.h>
#include <csp/csp_endian.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_client.h>

void vmem_download(int node, int timeout, uint32_t address, uint32_t length, char * dataout)
{
	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_RDP);
	if (conn == NULL)
		return;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	vmem_request_t * request = (void *) packet->data;
	request->version = 1;
	request->type = VMEM_SERVER_DOWNLOAD;
	request->address = csp_hton32(address);
	request->length = csp_hton32(length);
	packet->length = sizeof(vmem_request_t);

	/* Send request */
	if (!csp_send(conn, packet, timeout)) {
		csp_buffer_free(packet);
		csp_close(conn);
		return;
	}

	/* Go into download loop */
	while((packet = csp_read(conn, timeout)) != NULL) {
		csp_hex_dump("Response", packet->data, packet->length);
		csp_buffer_free(packet);
	}

	printf("No more data\n");
	csp_close(conn);
}

void vmem_upload(int node, int timeout, uint32_t address, char * datain, int len)
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
