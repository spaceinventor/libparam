/*
 * param_commands_client.c
 *
 *  Created on: Sep 22, 2021
 *      Author: Mads
 */

#include "param_commands_client.h"

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

typedef void (*param_transaction_callback_f)(csp_packet_t *response, int verbose, int version);
int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback, int verbose, int version);

static void param_transaction_callback_add(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_COMMAND_ADD_RESPONSE){
        return;
    }

	if (verbose) {
		if (csp_ntoh16(response->data16[1]) == UINT16_MAX) {
			printf("Adding command failed\n");
		} else {
			printf("Command added:\n", csp_ntoh16(response->data16[1]));
		}
	}

	csp_buffer_free(response);
}

int param_command_push(param_queue_t *queue, int verbose, int server, char command_name[], int timeout) {
	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	if (queue->version == 2) {
		packet->data[0] = PARAM_SCHEDULE_PUSH;
	} else {
		return -3;
	}

	packet->data[1] = 0;

	uint8_t name_length = (uint8_t) strlen(command_name);
	packet->data[2] = name_length;

	memcpy(&packet->data[3], command_name, name_length);

	memcpy(&packet->data[3+name_length], queue->buffer, queue->used);

	packet->length = queue->used + 3 + name_length;
	int result = param_transaction(packet, server, timeout, param_transaction_callback_add, verbose, queue->version);

	if (result < 0) {
		return -1;
	}

	if (verbose)
		param_queue_print(queue);

	return 0;
}

#if 0
int param_schedule_pull(param_queue_t *queue, int verbose, int server, uint16_t host, uint32_t time, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	if (queue->version == 2) {
		packet->data[0] = PARAM_SCHEDULE_PULL;
	} else {
		return -3;
	}

	packet->data[1] = 0;
    packet->data16[1] = csp_hton16(host);
    packet->data32[1] = csp_hton32(time);

	memcpy(&packet->data[8], queue->buffer, queue->used);

	packet->length = queue->used + 8;
	int result = param_transaction(packet, server, timeout, param_transaction_callback_add, verbose, queue->version);

	if (result < 0) {
		return -1;
	}

	if (verbose)
		param_queue_print(queue);

	return 0;
}
#endif
