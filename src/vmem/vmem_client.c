/*
 * vmem_client.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#include "vmem/vmem_compress.h"
#include <stdio.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>
#include <unistd.h>

#include <vmem/vmem_client.h>

static int abort = 0;

void vmem_client_abort(void) {
	abort = 1;
}

int vmem_download(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version, int use_rdp) {

	return vmem_download_progress(node, timeout, address, length, dataout, version, use_rdp, vmem_progress);

}

int vmem_download_progress(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version, int use_rdp, vmem_progress_cb cb)
{
	uint32_t time_begin = csp_get_ms();
	abort = 0;

	/* Establish RDP connection */
	uint32_t opts = CSP_O_CRC32;
	if (use_rdp) {
		opts |= CSP_O_RDP;
	}
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, opts);
	if (conn == NULL)
		return CSP_ERR_TIMEDOUT;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return CSP_ERR_NOBUFS;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_DOWNLOAD;
	if (version == 2) {
		request->data2.address = htobe64(address);
		request->data2.length = htobe32(length);
	} else {
		request->data.address = htobe32((uint32_t)(address & 0x00000000FFFFFFFFULL));
		request->data.length = htobe32(length);
	}
	packet->length = sizeof(vmem_request_t);

	/* Send request */
	csp_send(conn, packet);

	/* Go into download loop */
	uint32_t count = 0;

	while(1) { 

		if (count == length)
			break;

		/* Blocking read */
		packet = csp_read(conn, timeout);
		if (packet == NULL)
			break;

		if (abort) {
			csp_buffer_free(packet);
			break;
		}

		//csp_hex_dump("Download", packet->data, packet->length);

		/* RX overflow check */
		if (count + packet->length > length) {
			csp_buffer_free(packet);
			break;
		}
		if(cb) {
			cb(length, count);
		}

		/* Put data */
		memcpy((void *) ((intptr_t) dataout + count), packet->data, packet->length);

		/* Increment */
		count += packet->length;

		csp_buffer_free(packet);
	}

	printf(" - %.0f K\n", (count / 1024.0));

	csp_close(conn);

	uint32_t time_total = csp_get_ms() - time_begin;

	printf("  Downloaded %u bytes in %.03f s at %u Bps\n", (unsigned int) count, time_total / 1000.0, (unsigned int) (count / ((float)time_total / 1000.0)) );

	return count;

}

void vmem_progress(uint32_t total, uint32_t sofar) {

	if ((sofar / VMEM_SERVER_MTU) % 32 == 0) {
		printf("  ");
	}
	printf(".");
	fflush(stdout);
	if ((sofar / VMEM_SERVER_MTU + 1) % 32 == 0) {
		printf(" - %.0f K\n", (sofar / 1024.0));
	}
}

int vmem_upload(int node, int timeout, uint64_t address, char * datain, uint32_t length, int version) {

	return vmem_upload_progress(node, timeout, address, datain, length, version, vmem_progress);

}

int vmem_upload_progress(int node, int timeout, uint64_t address, char * datain, uint32_t length, int version, vmem_progress_cb cb) {

	abort = 0;

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL) {
		return CSP_ERR_TIMEDOUT;
	}

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return CSP_ERR_NOBUFS;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_UPLOAD;

	if (version == 2) {
		request->data2.address = htobe64(address);
		request->data2.length = htobe32(length);
	} else {
		request->data.address = htobe32((uint32_t)(address & 0x00000000FFFFFFFFULL));
		request->data.length = htobe32(length);
	}
	packet->length = sizeof(vmem_request_t);

	/* Send request */
	csp_send(conn, packet);

	uint32_t count = 0;
	while((count < length) && csp_conn_is_active(conn)) {

		if (abort) {
			csp_buffer_free(packet);
			break;
		}
		if(cb) {
			cb(length, count);
		}

		/* Prepare packet */
		csp_packet_t * packet = csp_buffer_get(VMEM_SERVER_MTU);
		packet->length = VMEM_MIN(VMEM_SERVER_MTU, length - count);

		/* Copy data */
		memcpy(packet->data, (void *) ((intptr_t) datain + count), packet->length);

		/* Increment */
		count += packet->length;

		csp_send(conn, packet);

	}

	csp_close(conn);

	return count;
}

static csp_packet_t * vmem_client_list_get(int node, int timeout, int version) {

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return NULL;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return NULL;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	csp_packet_t *resp = NULL;

	if (version == 3) {
		/* Allocate the maximum packet length to hold the response for the caller */
		resp = csp_buffer_get(CSP_BUFFER_SIZE);
		if (!resp) {
			printf("Could not allocate CSP buffer for VMEM response.\n");
			csp_close(conn);
			return NULL;
		}

		resp->length = 0;
		/* Keep receiving until we got everything or we got a timeout */
		while ((packet = csp_read(conn, timeout)) != NULL) {
			if (packet->data[0] & 0b01000000) {
				/* First packet */
				resp->length = 0;
			}

			/* Collect the response in the response packet */
			memcpy(&resp->data[resp->length], &packet->data[1], (packet->length - 1));
			resp->length += (packet->length - 1);

			if (packet->data[0] & 0b10000000) {
				/* Last packet, break the loop */
				csp_buffer_free(packet);
				break;
			}

			csp_buffer_free(packet);
		}

		if (packet == NULL) {
			printf("No response to VMEM list request\n");
		}
	} else {
		/* Wait for response */
		packet = csp_read(conn, timeout);
		if (packet == NULL) {
			printf("No response to VMEM list request\n");
		}
		resp = packet;
	}

	csp_close(conn);

	return resp;
}

void vmem_client_list(int node, int timeout, int version) {

	csp_packet_t * packet = vmem_client_list_get(node, timeout, version);
	if (packet == NULL) 
		return;

	if (version == 3) {
		for (vmem_list3_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			printf(" %2u: %-16.16s 0x%016"PRIX64" - %"PRIu64" typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), be64toh(vmem->size), vmem->type);
		}
	} else if (version == 2) {
		for (vmem_list2_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			printf(" %2u: %-5.5s 0x%016"PRIX64" - %"PRIu32" typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), be32toh(vmem->size), vmem->type);
		}
	} else {
		for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			printf(" %2u: %-5.5s 0x%08"PRIX32" - %"PRIu32" typ %u\r\n", vmem->vmem_id, vmem->name, be32toh(vmem->vaddr), be32toh(vmem->size), vmem->type);
		}

	}


	csp_buffer_free(packet);
}

int vmem_client_find(int node, int timeout, void * dataout, int version, char * name, int namelen) {
	csp_packet_t * packet = vmem_client_list_get(node, timeout, version);
	if (packet == NULL) 
		return -1;

	if (version == 3) {
		vmem_list3_t ret = {};
		for (vmem_list3_t * vmem = (void *)packet->data; (intptr_t)vmem < (intptr_t)packet->data + packet->length; vmem++) {
			if (strncmp(vmem->name, name, namelen) == 0) {
				ret.vmem_id = vmem->vmem_id;
				ret.type = vmem->type;
				memcpy(ret.name, vmem->name, 5);
				ret.vaddr = be64toh(vmem->vaddr);
				ret.size = be64toh(vmem->size);
			}
		}
		memcpy(dataout, &ret, sizeof(vmem_list3_t));
	} else if (version == 2) {
		vmem_list2_t ret = {};
		for (vmem_list2_t * vmem = (void *)packet->data; (intptr_t)vmem < (intptr_t)packet->data + packet->length; vmem++) {
			if (strncmp(vmem->name, name, namelen) == 0) {
				ret.vmem_id = vmem->vmem_id;
				ret.type = vmem->type;
				memcpy(ret.name, vmem->name, 5);
				ret.vaddr = be64toh(vmem->vaddr);
				ret.size = be32toh((uint32_t)(vmem->size & 0x00000000FFFFFFFFULL));
			}
		}
		memcpy(dataout, &ret, sizeof(vmem_list2_t));
	} else {
		vmem_list_t ret = {};
		for (vmem_list_t * vmem = (void *)packet->data; (intptr_t)vmem < (intptr_t)packet->data + packet->length; vmem++) {
			if (strncmp(vmem->name, name, namelen) == 0) {
				ret.vmem_id = vmem->vmem_id;
				ret.type = vmem->type;
				memcpy(ret.name, vmem->name, 5);
				ret.vaddr = be32toh((uint32_t)(vmem->vaddr & 0x00000000FFFFFFFFULL));
				ret.size = be32toh((uint32_t)(vmem->size & 0x00000000FFFFFFFFULL));
			}
		}
		memcpy(dataout, &ret, sizeof(vmem_list_t));
	}
	
	csp_buffer_free(packet);
	return 0;
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

int vmem_client_calc_crc32(int node, int timeout, uint64_t address, uint32_t length, uint32_t * crc_out, int version) {

	int res = -1;

	uint32_t time_begin = csp_get_ms();

	/* Establish connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return res;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return res;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_CALCULATE_CRC32;
	if (version == 2) {
		request->data2.address = htobe64(address);
		request->data2.length = htobe32(length);
	} else {
		request->data.address = htobe32((uint32_t)(address & 0x00000000FFFFFFFFULL));
		request->data.length = htobe32(length);
	}
	packet->length = sizeof(vmem_request_t);

	/* Send request */
	csp_send(conn, packet);

	/* Wait for the reponse from the server */
	/* Blocking read */
	packet = csp_read(conn, timeout);
	if (packet && packet->length >= sizeof(uint32_t)) {
		uint32_t crc;
		memcpy(&crc, &packet->data[0], sizeof(crc));
		(*crc_out) = be32toh(crc);
		csp_buffer_free(packet);
		res = 0;
		uint32_t time_total = csp_get_ms() - time_begin;
		printf("  Calculated CRC32 0x%08"PRIX32" on %u bytes in %.03f s\n", (*crc_out), (unsigned int) length, time_total / 1000.0);
	} else {
		/* Timeout !? */
		res = -2;
	}

	csp_close(conn);

	return res;
}

int vmem_client_codec(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint64_t length, int type, int version) {

	int res = -1;

	/* Establish connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return res;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL)
		return res;

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = type;
		request->data4.src_address = htobe64(src_address);
		request->data4.dst_address = htobe64(dst_address);
		request->data4.length = htobe64(length);
	packet->length = sizeof(vmem_request_t);

	printf("Codec success from 0x%08"PRIX64" to 0x%08"PRIX64" %u bytes\n", 
		src_address, dst_address, (unsigned int) length);

	/* Send request */
	csp_send(conn, packet);

	/* Wait for the reponse from the server */
	/* Blocking read */
	packet = csp_read(conn, timeout);
	if (packet) {
		if (packet->data[0] == VMEM_SERVER_SUCCESS) {
			res = 0;
		} else if (packet->data[0] == VMEM_SERVER_ENOSYS) {
			res = -3;
			printf("VMEM Codec not implemented on target\n");
		}
		csp_buffer_free(packet);
	} else {
		/* Timeout */
		res = -2;
	}

	csp_close(conn);

	return res;
}

int vmem_client_compress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint64_t length, int version) {
	return vmem_client_codec(node, timeout, src_address, dst_address, length, VMEM_SERVER_COMPRESS, version);
}

int vmem_client_decompress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint64_t length, int version) {
	return vmem_client_codec(node, timeout, src_address, dst_address, length, VMEM_SERVER_DECOMPRESS, version);
}


