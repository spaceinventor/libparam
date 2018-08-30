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

typedef void (*param_transaction_callback_f)(csp_packet_t *response);

static void param_transaction_callback_pull(csp_packet_t *response) {
	//csp_hex_dump("pull response", response->data, response->length);
	param_queue_t queue;
	param_queue_init(&queue, &response->data[2], response->length - 2, response->length - 2, PARAM_QUEUE_TYPE_SET);
	param_queue_apply(&queue);
	param_queue_print_local(&queue);
	csp_buffer_free(response);
}

static int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback) {

	//csp_hex_dump("transaction", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, host, PARAM_PORT_SERVER, 0, CSP_O_CRC32);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return -1;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -1;
	}

	if (timeout == -1) {
		csp_close(conn);
		return -1;
	}

	int result = -1;
	while((packet = csp_read(conn, timeout)) != NULL) {

		int end = (packet->data[1] == PARAM_FLAG_END);

		//csp_hex_dump("response", packet->data, packet->length);

		if (callback) {
			callback(packet);
		} else {
			csp_buffer_free(packet);
		}

		if (end) {
			result = 0;
			break;
		}

	}

	csp_close(conn);
	return result;
}

int param_pull_all(int verbose, int host, uint32_t mask, int timeout) {

	csp_packet_t *packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;
	packet->data[0] = PARAM_PULL_ALL_REQUEST;
	packet->data[1] = 0;
	packet->data32[1] = csp_hton32(mask);
	packet->length = 8;
	return param_transaction(packet, host, timeout, param_transaction_callback_pull);

}

int param_pull_queue(param_queue_t *queue, int verbose, int host, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PULL_REQUEST;
	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->buffer, queue->used);

	packet->length = queue->used + 2;
	return param_transaction(packet, host, timeout, param_transaction_callback_pull);

}


int param_pull_single(param_t *param, int offset, int verbose, int host, int timeout) {

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	packet->data[0] = PARAM_PULL_REQUEST;
	packet->data[1] = 0;

	param_queue_t queue;
	param_queue_init(&queue, &packet->data[2], PARAM_SERVER_MTU - 2, 0, PARAM_QUEUE_TYPE_GET);
	param_queue_add(&queue, param, offset, NULL);

	packet->length = queue.used + 2;
	return param_transaction(packet, host, timeout, param_transaction_callback_pull);
}


int param_push_queue(param_queue_t *queue, int verbose, int host, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_PUSH_REQUEST;
	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->buffer, queue->used);

	packet->length = queue->used + 2;
	int result = param_transaction(packet, host, timeout, NULL);

	if (result < 0) {
		return -1;
	}

	param_queue_print(queue);
	param_queue_apply(queue);

	return 0;
}

int param_push_single(param_t *param, int offset, void *value, int verbose, int host, int timeout) {

	// TODO: include unique packet id?
	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	packet->data[0] = PARAM_PUSH_REQUEST;
	packet->data[1] = 0;

	param_queue_t queue;
	param_queue_init(&queue, &packet->data[2], PARAM_SERVER_MTU - 2, 0, PARAM_QUEUE_TYPE_SET);
	param_queue_add(&queue, param, offset, value);

	packet->length = queue.used + 2;
	int result = param_transaction(packet, host, timeout, NULL);

	if (result < 0) {
		return -1;
	}

	param_set(param, offset, value);

	return 0;
}

