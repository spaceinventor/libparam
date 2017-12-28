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

static void param_serve_pull_request(csp_packet_t * request, int all) {

	csp_packet_t * response = NULL;
	param_queue_t q_response;

	int __allocate() {
		response = csp_buffer_get(PARAM_SERVER_MTU);
		if (response == NULL)
			return -1;
		param_queue_init(&q_response, &response->data[2], PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET);
		return 0;
	}

	void __send(int end) {
		response->data[0] = PARAM_PULL_RESPONSE;
		response->data[1] = (end) ? PARAM_FLAG_END : 0;
		response->length = q_response.used + 2;
		if (csp_sendto_reply(request, response, CSP_O_SAME, 0) != CSP_ERR_NONE) {
			csp_buffer_free(response);
			return;
		}
	}

	int __add(param_queue_t *queue, param_t * param, int offset, void *reader) {
		int result = param_queue_add(&q_response, param, offset, NULL);
		if (result != 0) {

			/* Flush */
			__send(0);
			if (__allocate() < 0)
				return 0;

			/* Retry on fresh buffer */
			if (param_queue_add(&q_response, param, offset, NULL) != 0) {
				printf("warn: param too big for mtu\n");
			}
		}
		return  0;
	}

	if (__allocate(&response, &q_response) < 0) {
		csp_buffer_free(request);
		return;
	}

	if (all == 0) {
		param_queue_t q_request;
		param_queue_init(&q_request, &request->data[2], request->length - 2, request->length - 2, PARAM_QUEUE_TYPE_SET);
		param_queue_foreach(&q_request, __add);
	} else {
		param_t * param;
		param_list_iterator i = {};
		while ((param = param_list_iterate(&i)) != NULL) {
			__add(&q_response, param, -1, NULL);
		}
	}

	__send(1);

	csp_buffer_free(request);

}

static void param_serve_push(csp_packet_t * packet, int send_ack)
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

	if (!csp_sendto_reply(packet, packet, CSP_O_SAME, 0))
		csp_buffer_free(packet);

}

void param_serve(csp_packet_t * packet) {
	switch(packet->data[0]) {
		case PARAM_PULL_REQUEST:
			param_serve_pull_request(packet, 0);
			break;

		case PARAM_PULL_ALL_REQUEST:
			param_serve_pull_request(packet, 1);
			break;

		case PARAM_PULL_RESPONSE:
			param_serve_push(packet, 0);
			break;

		case PARAM_PUSH_REQUEST:
			param_serve_push(packet, 1);
			break;

		default:
			printf("Unknown parameter request\n");
			csp_buffer_free(packet);
			break;
	}
}


