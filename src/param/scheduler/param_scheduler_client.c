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

typedef void (*param_transaction_callback_f)(csp_packet_t *response, int verbose, int version);
int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback, int verbose, int version);

static void param_transaction_callback_add(csp_packet_t *response, int verbose, int version) {
    if (response->data[0] != PARAM_SCHEDULE_ADD_RESPONSE){
        return;
    }

	//csp_hex_dump("pull response", response->data, response->length);
	if (verbose) {
		if (csp_ntoh16(response->data16[1]) == UINT16_MAX) {
			printf("Scheduling queue failed:\n");
		} else {
			printf("Queue scheduled with id %u:\n", csp_ntoh16(response->data16[1]));
		}
	}

	csp_buffer_free(response);
}

int param_schedule_push(param_queue_t *queue, int verbose, int server, uint16_t host, uint32_t time, int timeout) {

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

static void param_transaction_callback_show(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_SCHEDULE_SHOW_RESPONSE){
        return;
	}
    
	if (verbose) {
		int id = csp_ntoh16(response->data16[1]);
		if (id == UINT16_MAX) {
			printf("Requested schedule id not found.\n");
		} else {
			param_queue_t queue;
			param_queue_init(&queue, &response->data[10], response->length - 10, response->length - 10, response->data[8], version);
			int time = csp_ntoh32(response->data32[1]);
			//queue.last_node = response->id.src;

			/* Show the requested queue */
			printf("Showing queue id %u ", id);
			if (response->data[9] == 0) { // if not completed
				if (time <= 1E9) {
					printf("scheduled in %u s\n", time);
				} else {
					printf("scheduled at UNIX time: %u\n", time);
				}
			} else {
				if (time <= 1E9) {
					printf("completed %u s ago\n", time);
				} else {
					printf("completed at UNIX time: %u\n", time);
				}
			}

			param_queue_print(&queue);

			csp_hex_dump("Received queue metadata:", &queue, sizeof(queue));
			csp_hex_dump("Received queue buffer:", queue.buffer, queue.used);

		}
	}

	csp_buffer_free(response);
}

int param_show_schedule(int server, int verbose, uint16_t id, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_SHOW_REQUEST;
    packet->data[1] = 0;

    packet->data16[1] = csp_hton16(id);

	packet->length = 4;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_show, verbose, 2);

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
    
	if (verbose) {
		int num_scheduled = csp_ntoh16(response->data16[1]);
		/* List the entries */
		if (num_scheduled == 0) {
			printf("No scheduled queues\n");
		} else {
			printf("Received list of %u queues:\n", num_scheduled);
			for (int i = 0; i < num_scheduled; i++) {
				unsigned int idx = 4+i*(4+2+2);
				unsigned int time = csp_ntoh32(response->data32[idx/4]);
				if (response->data[idx+7] == PARAM_QUEUE_TYPE_SET) {
					printf("[SET] Queue id %u, ", csp_ntoh16(response->data16[idx/2+2]));
				} /*else {
					printf("[GET] Queue id %u, ", csp_ntoh16(response->data16[idx/2+2]));
				}*/
				if (response->data[idx+6] == 1) {
					if (time <= 1E9) {
						printf("completed %u s ago.\n", time);
					} else {
						printf("completed at UNIX time: %u.\n", time);
					}
				} else {
					if (time <= 1E9) {
						printf("scheduled in %u s.\n", time);
					} else {
						printf("scheduled at UNIX time: %u\n", time);
					}
				}
			}
		}
	}

	csp_buffer_free(response);
}

int param_list_schedule(int server, int verbose, int timeout) {
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_LIST_REQUEST;
    packet->data[1] = 0;

	packet->length = 2;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_list, verbose, 2);

    if (result < 0) {
		return -1;
	}

	return 0;
}

static void param_transaction_callback_rm(csp_packet_t *response, int verbose, int version) {
	//csp_hex_dump("pull response", response->data, response->length);
    if (response->data[0] != PARAM_SCHEDULE_RM_RESPONSE){
        return;
	}
    
	if (verbose) {
		if (csp_ntoh16(response->data16[1]) == UINT16_MAX) {
			//RM ALL RESPONSE
			printf("All scheduled command queues removed.\n");
		} else {
			//RM SINGLE RESPONSE
			printf("Schedule id %u removed.\n", csp_ntoh16(response->data16[1]));
		}
	}
    
	csp_buffer_free(response);
}

int param_rm_schedule(int server, int verbose, uint16_t id, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_RM_REQUEST;

    packet->data[1] = 0;

    packet->data16[1] = csp_hton16(id);
	
	packet->length = 4;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_rm, verbose, 2);

	if (result < 0) {
		return -1;
	}

	return 0;
}

int param_rm_all_schedule(int server, int verbose, int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	packet->data[0] = PARAM_SCHEDULE_RM_ALL_REQUEST;

    packet->data[1] = 0;

    packet->data16[1] = csp_hton16(UINT16_MAX);

	packet->length = 4;

    int result = param_transaction(packet, server, timeout, param_transaction_callback_rm, verbose, 2);

	if (result < 0) {
		return -1;
	}

	return 0;
}