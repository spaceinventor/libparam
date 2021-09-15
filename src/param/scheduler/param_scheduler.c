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

#include <objstore/objstore.h>

VMEM_DEFINE_FILE(schedule, "schedule", "schedule.cnf", 0x1000);

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

    /* Construct a temporary schedule structure */

    int queue_size = packet->length - 8;

    param_schedule_t * temp_schedule = malloc(sizeof(param_schedule_t) + queue_size);

    temp_schedule->active = 1;
    temp_schedule->completed = 0;

    last_id++;
    if (last_id == UINT16_MAX){
        last_id = 0;
    }
    //printf("schedule added at id: %d\n", last_id);
    temp_schedule->id = last_id;

    temp_schedule->time = csp_ntoh32(packet->data32[1]);
    if (temp_schedule->time > 1E9) {
        temp_schedule->timer_type = SCHED_TIMER_TYPE_UNIX;
    } else {
        temp_schedule->timer_type = SCHED_TIMER_TYPE_RELATIVE;
    }

    temp_schedule->host = csp_ntoh16(packet->data16[1]);
    param_queue_init(&temp_schedule->queue, temp_schedule + sizeof(param_schedule_t), packet->length - 8, packet->length - 8, q_type, 2);

    temp_schedule->buffer_ptr = packet;

    /* Determine schedule size and allocate VMEM */
    int obj_length = sizeof(param_schedule_t) + queue_size;

    int obj_offset = objstore_alloc(&vmem_schedule, obj_length, 1);

    objstore_write_obj(&vmem_schedule, obj_offset, OBJ_TYPE_SCHEDULE, obj_length, (void*) temp_schedule);
    
    csp_buffer_free(packet);
    free(temp_schedule);
    return last_id;
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

static int obj_offset_from_id_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int _id =  *(int*)ctx;
    printf(" Begin cb function\n");

    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;
    
    int length = objstore_read_obj_length(vmem, offset);
    param_schedule_t * temp_schedule = malloc(length);
    objstore_read_obj(vmem, offset, (void*) temp_schedule, 0);

    if (temp_schedule->id == _id) {
        free(temp_schedule);
        return -1;
    }

    free(temp_schedule);
    return 0;
}

static int obj_offset_from_id(vmem_t * vmem, int id) {
    printf(" Begin offset_from_id function\n");
    int offset;

    offset = objstore_scan(vmem, obj_offset_from_id_scancb, 0, (void*) &id);

    return offset;
}

int param_serve_schedule_show(csp_packet_t *packet) {
    printf("Starting serve_show function\n");
    uint16_t id = csp_ntoh16(packet->data16[1]);
    int offset = obj_offset_from_id(&vmem_schedule, id);
    int status = 0;
    //printf("Searching for id %d, idx: %d\n", id, idx);
    if (offset < 0) {
        //printf("No schedule with id %d found\n", id);
        status = -1;
    }
    
    if (status == 0) {
        printf("Reading schedule object");
        /* Read the schedule entry */
        int length = objstore_read_obj_length(&vmem_schedule, offset);
        param_schedule_t * temp_schedule = malloc(length);
        objstore_read_obj(&vmem_schedule, offset, (void*) temp_schedule, 0);

        temp_schedule->queue.buffer = (char*) (temp_schedule + sizeof(param_schedule_t));

        /* Respond with the requested schedule entry */
        packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data16[1] = csp_hton16(temp_schedule->id);
        packet->data32[1] = csp_hton32(temp_schedule->time);
        packet->data[8] = temp_schedule->queue.type;
        packet->data[9] = temp_schedule->completed;

        packet->length = temp_schedule->queue.used + 10;
        
        memcpy(&packet->data[10], temp_schedule->queue.buffer, temp_schedule->queue.used);

        free(temp_schedule);
    } else {
        /* Respond with an error code */
        packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data16[1] = csp_hton16(UINT16_MAX);

        packet->length = 4;
    }

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

    vmem_file_init(&vmem_schedule);

    // Initialize any un-initialized schedule entries
    for (int i = 0; i < SCHEDULE_LIST_LENGTH; i++) {
        //schedule_print(i);
        //param_queue_print(&schedule[i].queue);
    }
}