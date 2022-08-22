/*
 * vmem_client.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_client.h>

void vmem_download(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version)
{
	uint32_t time_begin = csp_get_ms();

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_DOWNLOAD;
	if (version == 2) {
		request->data2.address = htobe64(address);
		request->data2.length = htobe32(length);
	} else {
		request->data.address = htobe32((uint32_t)address);
		request->data.length = htobe32(length);
	}
	packet->length = sizeof(vmem_request_t);

	/* Send request */
	csp_send(conn, packet);

	/* Go into download loop */
	int count = 0;
	int dotcount = 0;
	while((packet = csp_read(conn, timeout)) != NULL) {

		//csp_hex_dump("Download", packet->data, packet->length);

		/* RX overflow check */
		if (count + packet->length > length) {
			csp_buffer_free(packet);
			break;
		}

		if (dotcount % 32 == 0)
			printf("  ");
		printf(".");
		fflush(stdout);
		dotcount++;
		if (dotcount % 32 == 0)
			printf(" - %.0f K\n", (count / 1024.0));

		/* Put data */
		memcpy((void *) ((intptr_t) dataout + count), packet->data, packet->length);

		/* Increment */
		count += packet->length;

		csp_buffer_free(packet);
	}

	printf(" - %.0f K\n", (count / 1024.0));

	csp_close(conn);

	uint32_t time_total = csp_get_ms() - time_begin;

	printf("  Downloaded %u bytes in %.03f s at %u Bps\n", (unsigned int) length, time_total / 1000.0, (unsigned int) (length / ((float)time_total / 1000.0)) );


}

void vmem_upload(int node, int timeout, uint64_t address, char * datain, uint32_t length, int version)
{
	uint32_t time_begin = csp_get_ms();

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_UPLOAD;

	if (version == 2) {
		request->data2.address = htobe64(address);
		request->data2.length = htobe32(length);
	} else {
		request->data.address = htobe32(address);
		request->data.length = htobe32(length);
	}
	packet->length = sizeof(vmem_request_t);

	/* Send request */
	csp_send(conn, packet);

	unsigned int count = 0;
	int dotcount = 0;
	while(count < length) {

		if (dotcount % 32 == 0)
			printf("  ");
		printf(".");
		fflush(stdout);
		dotcount++;
		if (dotcount % 32 == 0)
			printf(" - %.0f K\n", (count / 1024.0));

		/* Prepare packet */
		csp_packet_t * packet = csp_buffer_get(VMEM_SERVER_MTU);
		packet->length = VMEM_MIN(VMEM_SERVER_MTU, length - count);

		/* Copy data */
		memcpy(packet->data, (void *) ((intptr_t) datain + count), packet->length);

		/* Increment */
		count += packet->length;

		csp_send(conn, packet);

	}

	printf(" - %.0f K\n", (count / 1024.0));

	csp_close(conn);

	uint32_t time_total = csp_get_ms() - time_begin;

	printf("  Uploaded %u bytes in %.03f s at %u Bps\n", (unsigned int) length, time_total / 1000.0, (unsigned int) (length / ((float)time_total / 1000.0)) );

}

void vmem_client_list(int node, int timeout, int version) {

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		printf("No response\n");
		csp_close(conn);
		return;
	}

	if (request->version == 2) {
		for (vmem_list2_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			printf(" %u: %-5.5s 0x%"PRIx64" - %u typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
		}
	} else {
		for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			printf(" %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
		}

	}


	csp_buffer_free(packet);
	csp_close(conn);

}

int vmem_client_backup(int node, int vmem_id, int timeout, int backup_or_restore) {

	vmem_request_t request;
	request.version = VMEM_VERSION;
	if (backup_or_restore) {
		request.type = VMEM_SERVER_BACKUP;
	} else {
		request.type = VMEM_SERVER_RESTORE;
	}
	request.vmem.vmem_id = vmem_id;

	int8_t response = -1;
	if (!csp_transaction_w_opts(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, &request, sizeof(vmem_request_t), &response, 1, CSP_O_CRC32)) {
		return -2;
	}

	return (int) response;

}
