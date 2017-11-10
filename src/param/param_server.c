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
#include <param/param_client.h>
#include <param/param_list.h>
#include <param/param_queue.h>

#include <mpack/mpack.h>

#include "param_log.h"
#include "param_serializer.h"

void param_serve_pull_request(csp_conn_t * conn, csp_packet_t * request) {

	//csp_hex_dump("get handler", request->data, request->length);

	csp_packet_t * response = csp_buffer_get(256);
	if (response == NULL) {
		csp_buffer_free(request);
		return;
	}

	response->data[0] = PARAM_PUSH_REQUEST;
	response->data[1] = 0;

	param_queue_t queue;
	queue.extbuffer = (char *) &response->data[2];
	queue.type = PARAM_QUEUE_TYPE_SET;
	mpack_writer_init(&queue.writer, queue.extbuffer, 256-2);

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, (char *) &request->data[2], request->length - 2);
	while(reader.left > 0) {
		uint16_t short_id = mpack_expect_u16(&reader);
		if (mpack_reader_error(&reader) != mpack_ok)
			continue;
		param_t * param = param_list_find_id(param_parse_short_id_node(short_id), param_parse_short_id_paramid(short_id));
		if (param == NULL)
			continue;
		param_queue_push(&queue, param, NULL);
	}
	mpack_reader_destroy(&reader);
	csp_buffer_free(request);

	response->length = queue.writer.used + 2;
	//csp_hex_dump("get handler", response->data, response->length);

	if (!csp_send(conn, response, 0))
		csp_buffer_free(response);
}

static void param_serve_push(csp_conn_t * conn, csp_packet_t * packet)
{

	//csp_hex_dump("set handler", packet->data, packet->length);

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, (char *) &packet->data[2], packet->length - 2);

	size_t remaining;
	while((remaining = mpack_reader_remaining(&reader, NULL)) > 0) {
		param_deserialize_from_mpack(&reader);
		if (mpack_reader_error(&reader) != mpack_ok)
			break;
	}

	if (mpack_reader_destroy(&reader) != mpack_ok) {
		printf("<mpack parsing error %s>\n", mpack_error_to_string(mpack_reader_error(&reader)));
	}

	/* Send ack */
	memcpy(packet->data, "ok", 2);
	packet->length = 2;

	//csp_hex_dump("set handler", packet->data, packet->length);

	if (!csp_send(conn, packet, 0))
		csp_buffer_free(packet);

}

static void param_serve(csp_conn_t * conn, csp_packet_t * packet) {
	switch(packet->data[0]) {
		case PARAM_PULL_REQUEST:
			param_serve_pull_request(conn, packet);
			break;

		case PARAM_PULL_RESPONSE:
			param_pull_response(packet, 1);
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

