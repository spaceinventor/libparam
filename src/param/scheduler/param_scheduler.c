/*
 * param_scheduler.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#include "param_scheduler.h"

#include <stdio.h>
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>

#include <param/param.h>
#include <param/param_queue.h>
#include <param/param_server.h>
#include <param/param_list.h>



param_schedule_t schedule[SCHEDULE_LIST_LENGTH];
static uint16_t last_id = UINT16_MAX;

static uint16_t schedule_add(param_queue_t *queue, uint32_t time) {
    /* Find a free entry on the schedule list */
    int counter = 0;
    while(counter < SCHEDULE_LIST_LENGTH) {
        if ( (schedule[counter].active == 0) || (schedule[counter].active == 0xFF) )
            break;
    }
    if (counter >= SCHEDULE_LIST_LENGTH)
        return UINT16_MAX;

    schedule[counter].active = 1;
    schedule[counter].completed = 0;

    last_id++;
    if (last_id == UINT16_MAX)
        last_id++;
    schedule[counter].id = last_id;

    schedule[counter].time = time;

    schedule[counter].queue = *queue;

    return last_id;
}

static int get_schedule_idx(uint16_t id) {
    for (int i = 0; i < SCHEDULE_LIST_LENGTH; i++) {
        if ( (schedule[i].active == 0) || (schedule[i].active == 0xFF) ){
            continue;
        }
        if (schedule[i].id == id) {
            return i;
        }
    }
}

int param_serve_schedule_push(csp_packet_t *packet) {
    param_queue_t queue;
    param_queue_init(&queue, packet->data[6], packet->length - 6, packet->length - 6, PARAM_QUEUE_TYPE_SET, 2);

    uint16_t id = schedule_add(&queue, packet->data[2]);

    /* Send ack with ID */
	packet->data[0] = PARAM_SCHEDULE_ADD_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = id;
	packet->length = 4;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

int param_serve_schedule_pull(csp_packet_t *packet) {
    param_queue_t queue;
    param_queue_init(&queue, packet->data[6], packet->length - 6, packet->length - 6, PARAM_QUEUE_TYPE_GET, 2);

    uint16_t id = schedule_add(&queue, packet->data[2]);

    /* Send ack with ID */
	packet->data[0] = PARAM_SCHEDULE_ADD_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = id;
	packet->length = 4;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}


int param_serve_schedule_show(csp_packet_t *packet) {
    uint16_t id = packet->data16[1];
    int idx = get_schedule_idx(id);
    
    /* Respond with the requested schedule entry */
	packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = schedule[idx].id;
    packet->data32[1] = schedule[idx].time;
    packet->data[8] = schedule[idx].queue.type;
    
    memcpy(&packet->data[9], &schedule[idx].queue.buffer, schedule[idx].queue.used);
	packet->length = schedule[idx].queue.used + 9;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

int param_serve_schedule_list(csp_packet_t *packet) {
    unsigned int counter = 0;
    for (int i = 0; i < SCHEDULE_LIST_LENGTH; i++) {
        if ( (schedule[i].active == 0) || (schedule[i].active == 0xFF) ){
            continue;
        }
        unsigned int idx = 4+counter*(4+2+2);
        packet->data32[idx/4] = schedule[i].time;
        packet->data16[idx/2+2] = schedule[i].id;
        packet->data[idx+6] = schedule[i].completed;
        packet->data[idx+7] = schedule[i].queue.type;
        counter++;
    }

    packet->data[0] = PARAM_SCHEDULE_LIST_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = counter; // number of entries
	packet->length = counter*8 + 4;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}