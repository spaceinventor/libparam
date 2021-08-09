/*
 * param_scheduler_client.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#include "param_scheduler_client.h"

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

static void param_transaction_callback_add(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_SCHEDULE_ADD_RESPONSE){
        return;
    }

	//csp_hex_dump("pull response", response->data, response->length);
	if (response->data16[1] == UINT16_MAX) {
        printf("Schedule failed, scheduler memory full\n");
    } else {
        printf("Queue scheduled with id %d:\n", response->data16[1]);
    }

	csp_buffer_free(response);
}

int param_schedule_push(param_queue_t *queue, int verbose, int server, uint32_t time, int timeout) {

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
    packet->data16[1] = 0; // TODO: Optional ID input here to overwrite a specific ID?
    packet->data32[1] = time;

	memcpy(&packet->data[8], queue->buffer, queue->used);

	packet->length = queue->used + 8;
	int result = param_transaction(packet, server, timeout, param_transaction_callback_add, verbose, queue->version);

	if (result < 0) {
		return -1;
	}

	param_queue_print(queue);

	return 0;
}

int param_schedule_pull(param_queue_t *queue, int verbose, int server, uint32_t time, int timeout) {

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
    packet->data16[1] = 0; // TODO: Optional ID input here to overwrite a specific ID?
    packet->data32[1] = time;

	memcpy(&packet->data[8], queue->buffer, queue->used);

	packet->length = queue->used + 8;
	int result = param_transaction(packet, server, timeout, param_transaction_callback_add, verbose, queue->version);

	if (result < 0) {
		return -1;
	}

	param_queue_print(queue);

	return 0;
}

static void param_transaction_callback_show(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_SCHEDULE_SHOW_RESPONSE)
        return;
    
	param_queue_t queue;
	param_queue_init(&queue, &response->data[9], response->length - 9, response->length - 9, response->data[8], version);
	queue.last_node = response->id.src;

	/* Show the requested queue */
    if (response->data32[1] <= 1E9) {
        printf("Showing queue id %d scheduled in %d s\n", response->data16[1], response->data32[1]);
    } else {
        printf("Showing queue id %d scheduled at UNIX time: %d\n", response->data16[1], response->data32[1]);
    }

    param_queue_print(&queue);
    
	csp_buffer_free(response);
}

int param_show_schedule(int server, uint16_t id, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_SHOW_REQUEST;

    packet->data[1] = 0;

    packet->data16[1] = id;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_show, 0, 2);

	if (result < 0) {
		return -1;
	}

	return 0;
}

static void param_transaction_callback_list(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_SCHEDULE_LIST_RESPONSE){
        return;
    }
    
    int num_scheduled = response->data16[1];
    
    /* List the entries */
    printf("Received list of %d queues:\n", num_scheduled);
    for (int i = 0; i < num_scheduled; i++) {
        unsigned int idx = 4+i*(4+2+2);
        if (response->data[idx+7] == PARAM_QUEUE_TYPE_SET) {
            printf("[SET] Queue id %d,", response->data16[idx/2+2]);
        } else {
            printf("[GET] Queue id %d,", response->data16[idx/2+2]);
        }
        if (response->data[idx+6] == 1) {
            printf("completed %d s ago.\n", response->data32[idx/4]);
        } else {
            if (response->data32[idx/4] <= 1E9) {
                printf("scheduled in %d s.\n", response->data32[idx/4]);
            } else {
                printf("scheduled at UNIX time: %d\n", response->data32[idx/4]);
            }
        }
    }

	csp_buffer_free(response);
}

int param_list_schedule(int server, int timeout) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_LIST_REQUEST;

    packet->data[1] = 0;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_list, 0, 2);

    if (result < 0) {
		return -1;
	}

	return 0;
}