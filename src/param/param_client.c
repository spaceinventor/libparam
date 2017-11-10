/*
 * param_client.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>
#include <param/param_queue.h>

#include <mpack/mpack.h>

#include "param_serializer.h"
#include "param_string.h"

csp_packet_t * param_transaction(csp_packet_t *packet, int host, int timeout) {

	//csp_hex_dump("transaction", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_SERVER, 0, CSP_O_CRC32);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return NULL;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return NULL;
	}

	if (timeout == -1) {
		csp_close(conn);
		return NULL;
	}

	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		csp_close(conn);
		return NULL;
	}

	//csp_hex_dump("transaction response", packet->data, packet->length);

	csp_close(conn);
	return packet;
}

void param_pull_response(csp_packet_t * response, int verbose) {
	//csp_hex_dump("pull response", response->data, response->length);

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, (char *) &response->data[2], response->length - 2);
	while(reader.left > 0) {
		param_t *param = param_deserialize_from_mpack(&reader);
		if (mpack_reader_error(&reader) != mpack_ok)
			break;
		if (param && verbose)
			param_print(param, -1, NULL, 0, 1);
	}
	mpack_reader_destroy(&reader);
	csp_buffer_free(response);
}


int param_pull_queue(param_queue_t *queue, int verbose, int host, int timeout) {

	if (queue->writer.used == 0)
		return 0;

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PULL_REQUEST;
	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->writer.buffer, queue->writer.used);

	packet->length = queue->writer.used + 2;
	packet = param_transaction(packet, host, timeout);

	if (packet == NULL) {
		printf("No response\n");
		return -1;
	}

	param_pull_response(packet, verbose);
	return 0;
}


int param_pull_single(param_t *param, int verbose, int host, int timeout) {

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	packet->data[0] = PARAM_PULL_REQUEST;
	packet->data[1] = 0;

	mpack_writer_t writer;
	mpack_writer_init(&writer, (char *) &packet->data[2], 254);
	mpack_write_u16(&writer, param_get_short_id(param, 0, 0));
	mpack_writer_destroy(&writer);

	packet->length = writer.used + 2;
	packet = param_transaction(packet, host, timeout);

	if (packet == NULL) {
		printf("No response\n");
		return -1;
	}

	param_pull_response(packet, verbose);
	return 0;
}


int param_push_queue(param_queue_t *queue, int verbose, int host, int timeout) {

	if (queue->writer.used == 0)
		return 0;

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PUSH_REQUEST;
	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->writer.buffer, queue->writer.used);

	packet->length = queue->writer.used + 2;
	packet = param_transaction(packet, host, timeout);

	if (packet == NULL) {
		printf("No response\n");
		return -1;
	}

	printf("Set OK\n");
	param_queue_print(queue);
	csp_buffer_free(packet);

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, (char *) queue->writer.buffer, queue->writer.used);
	while(reader.left > 0) {
		param_deserialize_from_mpack(&reader);
	}
	mpack_reader_destroy(&reader);

	return 0;
}

int param_push_single(param_t *param, void *value, int verbose, int host, int timeout) {

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(256);
	packet->data[0] = PARAM_PUSH_REQUEST;
	packet->data[1] = 0;

	mpack_writer_t writer;
	mpack_writer_init(&writer, (char *) &packet->data[2], 254);
	param_serialize_to_mpack(param, &writer, value);

	packet->length = writer.used + 2;
	packet = param_transaction(packet, host, timeout);

	if (packet == NULL) {
		printf("No response\n");
		mpack_writer_destroy(&writer);
		return -1;
	}

	param_set(param, 0, value);
	csp_buffer_free(packet);

	mpack_writer_destroy(&writer);
	return 0;
}

