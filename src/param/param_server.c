/*
 * param_server.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>
#include <param/param.h>
#include <param/param_server.h>
#include <param/param_list.h>

#include <mpack/mpack.h>

#include "param_log.h"
#include "param_serializer.h"

void param_serve_pull_request(csp_conn_t * conn, csp_packet_t * request) {

	csp_hex_dump("get handler", request->data, request->length);

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, (char *) &request->data[2], request->length - 2);

	/* Expect an ext field */
	mpack_tag_t tag = mpack_read_tag(&reader);

	if (tag.type != mpack_type_ext || tag.v.l > PARAM_SERVER_MTU) {
		mpack_reader_destroy(&reader);
		csp_buffer_free(request);
		return;
	}

	uint8_t count = 0;
	param_t * params[100];

	for(int i = 0; i < tag.v.l / sizeof(uint16_t); i++) {
		uint16_t short_id;
		mpack_read_bytes(&reader, (char *) &short_id, sizeof(short_id));
		param_t * param = param_list_find_id(param_parse_short_id_node(short_id), param_parse_short_id_paramid(short_id));
		if (param == NULL)
			continue;
		params[count++] = param;
	}

	/* Reading is done */
	mpack_reader_destroy(&reader);

	/* Reuse CSP buffer */
	csp_packet_t * response = request;
	response->data[0] = PARAM_PULL_RESPONSE;
	response->data[1] = 0;

	mpack_writer_t writer;
	mpack_writer_init(&writer, (char *) &response->data[2], PARAM_SERVER_MTU - 2);

	/* Pack id's and data */
	mpack_start_map(&writer, count);
	for (int i = 0; i < count; i++) {
		param_serialize_to_mpack(params[i], &writer, NULL);
	}
	mpack_finish_map(&writer);

	mpack_print(writer.buffer, writer.used);

	response->length = writer.used + 2;

	csp_hex_dump("get handler", response->data, response->length);

	if (!csp_send(conn, response, 0))
		csp_buffer_free(response);
}

void param_serve_pull_response(csp_conn_t * conn, csp_packet_t * packet, int verbose) {

	csp_hex_dump("pull response", packet->data, packet->length);

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, (char *) &packet->data[2], packet->length - 2);

	mpack_print(reader.buffer, reader.left);

	/* Expect a map field */
	unsigned int count = mpack_expect_map(&reader);

	if (count == 0) {
		mpack_reader_destroy(&reader);
		csp_buffer_free(packet);
		return;
	}

	for(int i = 0; i < count; i++) {
		param_deserialize_from_mpack_map(&reader);
	}

	/* Reading is done */
	mpack_reader_destroy(&reader);

	csp_buffer_free(packet);
}

static void param_serve_push(csp_conn_t * conn, csp_packet_t * packet)
{

	csp_hex_dump("set handler", packet->data, packet->length);


#if 0
	uint32_t timestamp = csp_get_ms();
	uint8_t node = 255;

	uint8_t * input = &packet->data[2];
	while(input < packet->data + packet->length) {
		switch(*input) {
			case PARAM_CHUNK_TIME:
				input += param_deserialize_chunk_timestamp(&timestamp, input);
				break;
			case PARAM_CHUNK_NODE:
				input += param_deserialize_chunk_node(&node, input);
				break;
			case PARAM_CHUNK_PARAM_AND_VALUE: {
				input += param_deserialize_chunk_param_and_value(node, timestamp, 0, input);
				break;
			}
			default:
				printf("Invalid type %u\n", *input);
				csp_buffer_free(packet);
				return;
		}
	}

#endif

	/* Send ack */
	memcpy(packet->data, "ok", 2);
	packet->length = 2;

	csp_hex_dump("set handler", packet->data, packet->length);

	if (!csp_send(conn, packet, 0))
		csp_buffer_free(packet);

}

static void param_serve(csp_conn_t * conn, csp_packet_t * packet) {
	switch(packet->data[0]) {
		case PARAM_PULL_REQUEST:
			param_serve_pull_request(conn, packet);
			break;

		case PARAM_PULL_RESPONSE:
			param_serve_pull_response(conn, packet, 1);
			break;

		case PARAM_PUSH_REQUEST:
			param_serve_push(conn, packet);
			break;

		default:
			printf("Unknown parameter request\n");
			break;
	}
}

csp_thread_return_t param_server_task(void *pvParameters)
{

	/* Create socket without any socket options */
	csp_socket_t *sock = csp_socket(CSP_SO_NONE);

	/* Bind all ports to socket */
	csp_bind(sock, CSP_ANY);

	/* Create 10 connections backlog queue */
	csp_listen(sock, 10);

	/* Pointer to current connection and packet */
	csp_conn_t *conn;
	csp_packet_t *packet;

	/* Process incoming connections */
	while (1) {

		/* Wait for connection, 10000 ms timeout */
		if ((conn = csp_accept(sock, CSP_MAX_DELAY)) == NULL)
			continue;

		/* Read packets. Timout is 100 ms */
		while ((packet = csp_read(conn, 0)) != NULL) {
			switch (csp_conn_dport(conn)) {

			case PARAM_PORT_SERVER:
				param_serve(conn, packet);
				break;

			default:
				/* Let the service handler reply pings, buffer use, etc. */
				csp_service_handler(conn, packet);
				break;
			}
		}

		/* Close current connection, and handle next */
		csp_close(conn);

	}

	return CSP_TASK_RETURN;

}

