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
#include <param/param_queue.h>
#include <param/param_server.h>
#include <param/param_list.h>

static void param_serve_pull_all_request(csp_conn_t * conn, csp_packet_t * request) {

	csp_buffer_free(request);
	csp_packet_t * response = NULL;
	param_queue_t q_response;

	void allocate(void) {
		response = csp_buffer_get(PARAM_SERVER_MTU);
		if (response == NULL)
			return;
		param_queue_init(&q_response, &response->data[2], PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET);
	}

	void send(int end) {
		response->data[0] = PARAM_PULL_RESPONSE;
		response->data[1] = (end) ? PARAM_FLAG_END : 0;
		response->length = q_response.used + 2;
		if (!csp_send(conn, response, 0)) {
			csp_buffer_free(response);
			return;
		}
	}

	allocate();

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		int result = param_queue_add(&q_response, param, NULL);
		if (result != 0) {

			/* Flush */
			send(0);
			allocate();

			/* Retry on fresh buffer */
			if (param_queue_add(&q_response, param, NULL) != 0) {
				printf("warn: param too big for mtu\n");
			}
		}
	}

	send(1);

}

static void param_serve_pull_request(csp_conn_t * conn, csp_packet_t * request) {

	//csp_hex_dump("get handler", request->data, request->length);

	csp_packet_t * response = csp_buffer_get(PARAM_SERVER_MTU);
	if (response == NULL) {
		csp_buffer_free(request);
		return;
	}

	response->data[0] = PARAM_PULL_RESPONSE;
	response->data[1] = PARAM_FLAG_END;

	param_queue_t q_response;
	param_queue_init(&q_response, &response->data[2], PARAM_SERVER_MTU-2, 0, PARAM_QUEUE_TYPE_SET);
	param_queue_t q_request;
	param_queue_init(&q_request, &request->data[2], PARAM_SERVER_MTU-2, request->length - 2, PARAM_QUEUE_TYPE_SET);

	int add_callback(param_queue_t *queue, param_t * param, void *reader) {
		param_queue_add(&q_response, param, NULL);
		return  0;
	}
	param_queue_foreach(&q_request, add_callback);
	csp_buffer_free(request);

	response->length = q_response.used + 2;
	//csp_hex_dump("get handler", response->data, response->length);

	if (!csp_send(conn, response, 0))
		csp_buffer_free(response);
}

static void param_serve_push(csp_conn_t * conn, csp_packet_t * packet, int send_ack)
{
	//csp_hex_dump("set handler", packet->data, packet->length);

	param_queue_t queue;
	param_queue_init(&queue, &packet->data[2], packet->length - 2, packet->length - 2, PARAM_QUEUE_TYPE_SET);
	int result = param_queue_apply(&queue);

	if ((result != 0) || (send_ack == 0)) {
		csp_buffer_free(packet);
		return;
	}

	/* Send ack */
	packet->data[0] = PARAM_PUSH_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
	packet->length = 2;

	if (!csp_send(conn, packet, 0))
		csp_buffer_free(packet);

}

static void param_serve(csp_conn_t * conn, csp_packet_t * packet) {
	switch(packet->data[0]) {
		case PARAM_PULL_REQUEST:
			param_serve_pull_request(conn, packet);
			break;

		case PARAM_PULL_ALL_REQUEST:
			param_serve_pull_all_request(conn, packet);
			break;

		case PARAM_PULL_RESPONSE:
			param_serve_push(conn, packet, 0);
			break;

		case PARAM_PUSH_REQUEST:
			param_serve_push(conn, packet, 1);
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

