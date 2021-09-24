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

static void name_copy(char output[], char input[], int length) {
    for (int i = 0; i < length; i++) {
			output[i] = input[i];
    }
    output[length] = '\0';
}

static void param_transaction_callback_show(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_COMMAND_SHOW_RESPONSE){
        return;
	}
    
	if (verbose) {
		int name_length = csp_ntoh16(response->data16[1]);
		if (name_length > 13) {
			printf("Error: Requested command name not found.\n");
		} else {
			char name[14];
			name_copy(name, &response->data[4], name_length);

			param_queue_t queue;
			param_queue_init(&queue, &response->data[4+name_length], response->length - (4+name_length), response->length - (4+name_length), response->data[3], version);

			/* Show the requested queue */
			printf("Showing command, name: %s\n", name);

			param_queue_print(&queue);
		}
	}

	csp_buffer_free(response);
}

int param_show_command(int server, int verbose, char command_name[], int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_COMMAND_SHOW_REQUEST;
    packet->data[1] = 0;

    int name_length = strlen(command_name);

	packet->data[2] = name_length;

	memcpy(&packet->data[3], command_name, name_length);

	packet->length = 3+name_length;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_show, verbose, 2);

	if (result < 0) {
		return -1;
	}

	return 0;
}