/*
 * param_scheduler.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#include <param/param_scheduler.h>

#include <stdio.h>
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>

#include <param/param.h>
#include <param/param_queue.h>
#include <param/param_server.h>
#include <param/param_list.h>
#include <param/param_client.h>


param_schedule_t schedule[SCHEDULE_LIST_LENGTH];
static uint16_t last_id = UINT16_MAX;

#if 0
/* Debug schedule print function */
static void schedule_print(int idx) {
    if (idx < 0)
        return;

    printf("Schedule idx %d, id %d for host %d:\n", idx, schedule[idx].id, schedule[idx].host);
    printf(" Status: Active (%d) - Completed (%d)\n", schedule[idx].active, schedule[idx].completed);
    if (schedule[idx].active == 1) {
        if (schedule[idx].completed == 1) {
            if (schedule[idx].timer_type == SCHED_TIMER_TYPE_RELATIVE) {
                printf(" Completed %d s ago\n", schedule[idx].time);
            } else {
                printf(" Completed at UNIX time %d\n", schedule[idx].time);
            }
        } else {
            if (schedule[idx].timer_type == SCHED_TIMER_TYPE_RELATIVE) {
                printf(" Scheduled in %d s\n", schedule[idx].time);
            } else {
                printf(" Scheduled at UNIX time %d\n", schedule[idx].time);
            }
        }

    } else {
        printf(" Inactive with time = %d and timer_type = %d\n", schedule[idx].time, schedule[idx].timer_type);
    }

}
#endif

static uint16_t schedule_add(csp_packet_t *packet, param_queue_type_e q_type) {

    /* Find a free entry on the schedule list */
    int counter = 0;
    while(counter < SCHEDULE_LIST_LENGTH) {
        if ( (schedule[counter].active == 0) || (schedule[counter].active == 0xFF) )
            break;
        counter++;
    }
    if (counter >= SCHEDULE_LIST_LENGTH)
        return UINT16_MAX;

    schedule[counter].active = 1;
    schedule[counter].completed = 0;

    last_id++;
    if (last_id == UINT16_MAX){
        last_id = 0;
    }
    //printf("schedule added at id: %d\n", last_id);
    schedule[counter].id = last_id;

    schedule[counter].time = csp_ntoh32(packet->data32[1]);
    if (schedule[counter].time > 1E9) {
        schedule[counter].timer_type = SCHED_TIMER_TYPE_UNIX;
    } else {
        schedule[counter].timer_type = SCHED_TIMER_TYPE_RELATIVE;
    }

    schedule[counter].host = csp_ntoh16(packet->data16[1]);
    param_queue_init(&schedule[counter].queue, &packet->data[8], packet->length - 8, packet->length - 8, q_type, 2);

    schedule[counter].buffer_ptr = packet;

    return schedule[counter].id;
}

static int get_schedule_idx(uint16_t id) {
    for (int i = 0; i < SCHEDULE_LIST_LENGTH; i++) {
        if ( (schedule[i].active == 0) || (schedule[i].active == 0xFF) ) {
            continue;
        }
        if (schedule[i].id == id) {
            return i;
        }
    }
    return -1;
}

int param_serve_schedule_push(csp_packet_t *request) {
    csp_packet_t * response = csp_buffer_get(0);
    if (response == NULL) {
        csp_buffer_free(request);
        return -1;
    }

    csp_packet_t * request_copy = csp_buffer_clone(request);
    uint16_t id = schedule_add(request_copy, PARAM_QUEUE_TYPE_SET);

    /* Send ack with ID */
	response->data[0] = PARAM_SCHEDULE_ADD_RESPONSE;
	response->data[1] = PARAM_FLAG_END;
    response->data16[1] = csp_hton16(id);
	response->length = 4;

	if (csp_sendto_reply(request, response, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(response);

    csp_buffer_free(request);

    //int idx = get_schedule_idx(id);
    //schedule_print(idx);

    return 0;
}

#if 0
int param_serve_schedule_pull(csp_packet_t *request) {
    csp_packet_t * response = csp_buffer_get(0);
    if (response == NULL) {
        csp_buffer_free(request);
        return -1;
    }

    csp_packet_t * request_copy = csp_buffer_clone(request);
    uint16_t id = schedule_add(request_copy, PARAM_QUEUE_TYPE_GET);

    /* Send ack with ID */
	response->data[0] = PARAM_SCHEDULE_ADD_RESPONSE;
	response->data[1] = PARAM_FLAG_END;
    response->data16[1] = csp_hton16(id);
	response->length = 4;

	if (csp_sendto_reply(request, response, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(response);

    csp_buffer_free(request);

    return 0;
}
#endif

int param_serve_schedule_show(csp_packet_t *packet) {
    uint16_t id = csp_ntoh16(packet->data16[1]);
    int idx = get_schedule_idx(id);
    int status = 0;
    //printf("Searching for id %d, idx: %d\n", id, idx);
    if (idx < 0) {
        //printf("No active schedule with id %d found\n", id);
        status = -1;
    }
    
    if (status == 0) {
        /* Respond with the requested schedule entry */
        //printf("Building show response packet:\n");
        packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
        //printf(" type: %d\n", packet->data[0]);
        packet->data[1] = PARAM_FLAG_END;
        //printf(" end flag: %d\n", packet->data[1]);
        packet->data16[1] = csp_hton16(schedule[idx].id);
        //printf(" schedule id: %d\n", csp_ntoh16(packet->data16[1]));
        packet->data32[1] = csp_hton32(schedule[idx].time);
        //printf(" schedule time: %u\n", csp_ntoh32(packet->data32[1]));
        packet->data[8] = schedule[idx].queue.type;
        packet->data[9] = schedule[idx].completed;
        //printf(" queue type: %u\n", packet->data[8]);
        //printf(" queue length: %d\n", schedule[idx].queue.used);
        packet->length = schedule[idx].queue.used + 10;
        //csp_hex_dump("show response packet to transmit", &packet->data, packet->length);
        
        memcpy(&packet->data[10], schedule[idx].queue.buffer, schedule[idx].queue.used);
        //printf(" total packet length: %d\n", packet->length);
    } else {
        /* Respond with an error code */
        packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data16[1] = csp_hton16(UINT16_MAX);

        packet->length = 4;
    }

    //printf("Queue size: %d bytes\n", schedule[idx].queue.used);
    //param_queue_print(&schedule[idx].queue);
    //csp_hex_dump("queue to transmit", schedule[idx].queue.buffer, schedule[idx].queue.used);
    //csp_hex_dump("show response packet to transmit (after memcpy)", &packet->data, packet->length);

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

int param_serve_schedule_rm_single(csp_packet_t *packet) {
    /* Disable the specified schedule id */
    uint16_t id = csp_ntoh16(packet->data16[1]);
    int idx = get_schedule_idx(id);
    if (idx < 0) {
        return -1;
        csp_buffer_free(packet);
    }
    schedule[idx].active = 0,
    csp_buffer_free(schedule[idx].buffer_ptr);
    
    /* Respond with the id again to verify which ID was disabled */
	packet->data[0] = PARAM_SCHEDULE_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = csp_hton16(schedule[idx].id);
    
	packet->length = 4;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

int param_serve_schedule_rm_all(csp_packet_t *packet) {
    /* Confirm remove all by checking that id = UINT16_MAX */
    uint16_t id = csp_ntoh16(packet->data16[1]);
    if (id != UINT16_MAX) {
        return -1;
        csp_buffer_free(packet);
    }
    /* Clear the schedule by setting all entries inactive */
    for (int i = 0; i < SCHEDULE_LIST_LENGTH; i++) {
        schedule[i].active = 0;
        csp_buffer_free(schedule[i].buffer_ptr);
    }

    /* Respond with id = UINT16_MAX again to verify that the schedule was cleared */
	packet->data[0] = PARAM_SCHEDULE_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = csp_hton16(UINT16_MAX);
    
	packet->length = 4;

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
        packet->data32[idx/4] = csp_hton32(schedule[i].time);
        packet->data16[idx/2+2] = csp_hton16(schedule[i].id);
        packet->data[idx+6] = schedule[i].completed;
        packet->data[idx+7] = schedule[i].queue.type;
        counter++;
    }

    packet->data[0] = PARAM_SCHEDULE_LIST_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = csp_hton16(counter); // number of entries
	packet->length = counter*8 + 4;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

uint32_t last_timestamp = 0;

int param_schedule_server_update(uint32_t timestamp) {
    //printf("Schedule server update started.\n");
    int dt;
    if (last_timestamp == 0) {
        dt = 1;
    } else {
        dt = (int) timestamp - last_timestamp;
    }
    if (dt < 0) {
        return -1;
    }
    last_timestamp = timestamp;

    for (int i = 0; i < SCHEDULE_LIST_LENGTH; i++) {
        if ( (schedule[i].active == 0) || (schedule[i].active == 0xFF) )
            continue;

        //printf("Checking schedule idx %d...\n", i);
        //schedule_print(i);

        if (schedule[i].timer_type == SCHED_TIMER_TYPE_RELATIVE) {
            for (int j = 0; j < dt; j++) {
                if (schedule[i].completed != 1) {
                    if (schedule[i].time != 0)
                        schedule[i].time--;
                    
                    if (schedule[i].time == 0) {
                        /* Execute the scheduled queue */
                        if (param_push_queue(&schedule[i].queue, 0, schedule[i].host, 100) == 0) {
                            param_queue_print(&schedule[i].queue);
                            schedule[i].completed = 1;
                        } else {
                            /* Postpone 10 seconds to retry in case of network errors */
                            schedule[i].time = 10;
                            break;
                        }
                    }
                } else {
                    schedule[i].time++;
                    if (schedule[i].time >= 86400) {
                        /* Deactivate completed schedule entries after 24 hrs */
                        schedule[i].active = 0;
                        csp_buffer_free(schedule[i].buffer_ptr);
                        break;
                    }
                }
            }
        } else {
            if (schedule[i].completed != 1) {
                if (schedule[i].time >= (timestamp)) {
                    /* Execute the scheduled queue */
                    if (param_push_queue(&schedule[i].queue, 0, schedule[i].host, 100) == 0){
                        schedule[i].completed = 1;
                    } else {
                        /* Postpone 10 seconds to retry in case of network errors */
                        schedule[i].time += 10;
                    }
                }
            } else {
                if (schedule[i].time >= (timestamp + 86400)) {
                    /* Deactivate completed schedule entries after 24 hrs */
                    schedule[i].active = 0;
                    csp_buffer_free(schedule[i].buffer_ptr);
                }
            }
        }
    }

    //printf("Schedule server update completed.\n");
    return 0;
}

void param_schedule_server_init(void) {
    // Allocate the schedule array in FRAM or check if it's already there

    // Initialize any un-initialized schedule entries
    for (int i = 0; i < SCHEDULE_LIST_LENGTH; i++) {
        //schedule_print(i);
        //param_queue_print(&schedule[i].queue);
    }
}