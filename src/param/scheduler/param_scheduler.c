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

param_scheduler_meta_t meta_obj;

#if 0
/* Debug schedule print function, not updated to objstore */
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

static int find_meta_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);

    if (type == OBJ_TYPE_SCHEDULER_META)
        return -1;

    return 0;
}

static void meta_obj_save(vmem_t * vmem) {
    int offset = objstore_scan(vmem, find_meta_scancb, 0, NULL);
    if (offset < 0)
        return;
    
    objstore_write_obj(vmem, offset, OBJ_TYPE_SCHEDULER_META, sizeof(meta_obj), (void *) &meta_obj);
}

static uint16_t schedule_add(csp_packet_t *packet, param_queue_type_e q_type) {
    /* Construct a temporary schedule structure */
    if (packet->length < 12)
        return UINT16_MAX;
    
    int queue_size = packet->length - 12;

    /* Determine schedule size and allocate VMEM */
    int obj_length = (int) sizeof(param_schedule_t) + queue_size - (int) sizeof(char *);
    if (obj_length < 0)
        return UINT16_MAX;

    int obj_offset = objstore_alloc(&vmem_schedule, obj_length, 0);
    if (obj_offset < 0)
        return UINT16_MAX;

    param_schedule_t * temp_schedule = malloc(sizeof(param_schedule_t) + queue_size);

    temp_schedule->active = 0x55;
    temp_schedule->completed = 0;

    int meta_offset = objstore_scan(&vmem_schedule, find_meta_scancb, 0, NULL);
    if (meta_offset < 0)
        return UINT16_MAX;
    
    if (objstore_read_obj(&vmem_schedule, meta_offset, (void *) &meta_obj, 0) < 0)
        printf("Meta obj checksum error\n");
    meta_obj.last_id++;
    if (meta_obj.last_id == UINT16_MAX){
        meta_obj.last_id = 0;
    }
    meta_obj_save(&vmem_schedule);
    temp_schedule->id = meta_obj.last_id;

    uint64_t clock_get_nsec(void);
	uint64_t timestamp = clock_get_nsec();

    temp_schedule->time = (uint64_t) csp_ntoh32(packet->data32[1])*1E9;
    if (temp_schedule->time <= 1E18) {
        temp_schedule->time += timestamp;
    }
    temp_schedule->latency_buffer = csp_ntoh32(packet->data32[2]);

    temp_schedule->host = csp_ntoh16(packet->data16[1]);

    /* Initialize schedule queue and copy queue buffer from CSP packet */
    param_queue_init(&temp_schedule->queue, (char *) temp_schedule + sizeof(param_schedule_t), queue_size, queue_size, q_type, 2);
    memcpy(temp_schedule->queue.buffer, &packet->data[12], temp_schedule->queue.used);

    void * write_ptr = (void *) (long int) temp_schedule + sizeof(temp_schedule->queue.buffer);
    objstore_write_obj(&vmem_schedule, obj_offset, (uint8_t) OBJ_TYPE_SCHEDULE, (uint8_t) obj_length, write_ptr);
    
    free(temp_schedule);
    return meta_obj.last_id;
}

int param_serve_schedule_push(csp_packet_t *request) {
    csp_packet_t * response = csp_buffer_get(4);
    if (response == NULL) {
        csp_buffer_free(request);
        return -1;
    }

    uint16_t id = schedule_add(request, PARAM_QUEUE_TYPE_SET);

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

#if 0
// not updated with objstore
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
    int target_id =  *(int*)ctx;

    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;

    uint16_t id;
    vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,id), &id, sizeof(id));
    
    if (id == target_id) {
        return -1;
    }

    return 0;
}

static int obj_offset_from_id(vmem_t * vmem, int id) {
    int offset = objstore_scan(vmem, obj_offset_from_id_scancb, 0, (void*) &id);
    return offset;
}

int param_serve_schedule_show(csp_packet_t *packet) {
    uint16_t id = csp_ntoh16(packet->data16[1]);
    int status = 0;
    int offset = obj_offset_from_id(&vmem_schedule, id);
    if (offset < 0) {
        status = -1;
    }
    int length = objstore_read_obj_length(&vmem_schedule, offset);
    if (length < 0) {
        status = -1;
    }
    
    if (status == 0) {
        /* Read the schedule entry */
        param_schedule_t * temp_schedule = malloc(length + sizeof(temp_schedule->queue.buffer));
        void * read_ptr = (void*) ( (long int) temp_schedule + sizeof(temp_schedule->queue.buffer));
        objstore_read_obj(&vmem_schedule, offset, read_ptr, 0);

        temp_schedule->queue.buffer = (char *) ((long int) temp_schedule + (long int) (sizeof(param_schedule_t)));

        /* Respond with the requested schedule entry */
        packet->data[0] = PARAM_SCHEDULE_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data16[1] = csp_hton16(temp_schedule->id);
        uint32_t time_s = (uint32_t) (temp_schedule->time / 1E9);
        packet->data32[1] = csp_hton32(time_s);
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
    int offset = obj_offset_from_id(&vmem_schedule, id);
    if (offset < 0) {
        return -1;
        csp_buffer_free(packet);
    }

    if (objstore_rm_obj(&vmem_schedule, offset, 0) < 0) {
        return -1;
        csp_buffer_free(packet);
    }
    
    /* Respond with the id again to verify which ID was erased */
	packet->data[0] = PARAM_SCHEDULE_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = csp_hton16(id);
    
	packet->length = 4;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

static int num_schedule_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;

    /* Found a schedule object, increment ctx */
    *(int*)ctx += 1;

    return 0;
}

static int get_number_of_schedule_objs(vmem_t * vmem) {
    int num_schedule_objs = 0;

    objstore_scan(vmem, num_schedule_scancb, 0, (void *) &num_schedule_objs);

    return num_schedule_objs;
}

static int next_schedule_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;
    
    int length = objstore_read_obj_length(vmem, offset);
    if (length < 0)
        return 0;

    /* Break iteration after the correct number of objects */
    if (*(int*)ctx == 0) {
        return -1;
    } else {
        *(int*)ctx -= 1;
    }

    return 0;
}

int param_serve_schedule_rm_all(csp_packet_t *packet) {
    /* Confirm remove all by checking that id = UINT16_MAX */
    uint16_t id = csp_ntoh16(packet->data16[1]);
    if (id != UINT16_MAX) {
        return -1;
        csp_buffer_free(packet);
    }
    int num_schedules = get_number_of_schedule_objs(&vmem_schedule);
    uint16_t deleted_schedules = 0;
    /* Iterate over the number of schedules and erase each */
    for (int i = 0; i < num_schedules; i++) {
        int obj_skips = 0;
        int offset = objstore_scan(&vmem_schedule, next_schedule_scancb, 0, (void *) &obj_skips);
        if (offset < 0) {
            continue;
        }

        if (objstore_rm_obj(&vmem_schedule, offset, 0) < 0) {
            continue;
        }
        deleted_schedules++;
    }

    /** Respond with id = UINT16_MAX again to verify that the schedule was cleared
     * include the number of schedules deleted in the response
     */
	packet->data[0] = PARAM_SCHEDULE_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = csp_hton16(UINT16_MAX);
    packet->data16[2] = csp_hton16(deleted_schedules);

	packet->length = 6;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

int param_serve_schedule_list(csp_packet_t *request) {
    int num_schedules = get_number_of_schedule_objs(&vmem_schedule);
    unsigned int counter = 0;
    unsigned int big_count = 0;
    int end = 0;

    while (end == 0) {
        csp_packet_t * response = csp_buffer_get(PARAM_SERVER_MTU);
        if (response == NULL)
            break;

        int num_per_packet = (PARAM_SERVER_MTU-4)/8;
        
        if ( (num_schedules - big_count*num_per_packet)*8 < (PARAM_SERVER_MTU - 4))
            end = 1;

        int loop_max = (big_count+1)*num_per_packet;
        if (end == 1)
            loop_max = num_schedules;

        for (int i = big_count*num_per_packet; i < loop_max; i++) {
            int obj_skips = counter;
            int offset = objstore_scan(&vmem_schedule, next_schedule_scancb, 0, (void *) &obj_skips);
            if (offset < 0) {
                counter++;
                continue;
            }

            int length = objstore_read_obj_length(&vmem_schedule, offset);
            if (length < 0) {
                counter++;
                continue;
            }
            param_schedule_t * temp_schedule = malloc(length + sizeof(temp_schedule->queue.buffer));
            void * read_ptr = (void*) ( (long int) temp_schedule + sizeof(temp_schedule->queue.buffer));
            objstore_read_obj(&vmem_schedule, offset, read_ptr, 0);

            unsigned int idx = 4+(counter-big_count*num_per_packet)*(4+2+2);

            uint32_t time_s = (uint32_t) (temp_schedule->time / 1E9);
            response->data32[idx/4] = csp_hton32(time_s);
            response->data16[idx/2+2] = csp_hton16(temp_schedule->id);
            response->data[idx+6] = temp_schedule->completed;
            response->data[idx+7] = temp_schedule->queue.type;
            
            free(temp_schedule);
            
            counter++;
        }

        response->data[0] = PARAM_SCHEDULE_LIST_RESPONSE;
        if (end == 1) {
            response->data[1] = PARAM_FLAG_END;
        } else {
            response->data[1] = 0;
        }
        response->data16[1] = csp_hton16(counter-big_count*num_per_packet); // number of entries
        response->length = (counter-big_count*num_per_packet)*8 + 4;

        if (csp_sendto_reply(request, response, CSP_O_SAME, 0) != CSP_ERR_NONE)
            csp_buffer_free(response);

        big_count++;
    }

    csp_buffer_free(request);

    return 0;
}

void param_serve_schedule_reset(csp_packet_t *packet) {
    if (csp_ntoh16(packet->data16[1]) != 0) {
        meta_obj.last_id = csp_ntoh16(packet->data16[1]);
    }

    meta_obj_save(&vmem_schedule);

    packet->data[0] = PARAM_SCHEDULE_RESET_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;

    packet->data16[1] = csp_hton16(meta_obj.last_id);
	
    packet->length = 4;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);
}

static int find_inactive_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_SCHEDULE)
        return 0;
    
    int length = objstore_read_obj_length(vmem, offset);
    if (length < 0)
        return 0;

    uint8_t active;
    vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, active), &active, sizeof(active));

    if (active == 0) {
        return -1;
    }

    return 0;
}

int param_schedule_server_update(void) {

    uint64_t clock_get_nsec(void);
	uint64_t timestamp = clock_get_nsec();

    int num_schedules = get_number_of_schedule_objs(&vmem_schedule);
    /* Check the time on each schedule and execute if deadline is exceeded */ 
    for (int i = 0; i < num_schedules; i++) {
        int obj_skips = i;
        int offset = objstore_scan(&vmem_schedule, next_schedule_scancb, 0, (void *) &obj_skips);
        if (offset < 0) {
            continue;
        }

        uint8_t completed;
        vmem_schedule.read(&vmem_schedule, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,completed), &completed, sizeof(completed));
        uint64_t time;
        vmem_schedule.read(&vmem_schedule, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,time), &time, sizeof(time));

        if (completed == 0) {
            if (time <= timestamp) {
                uint32_t latency_buffer;
                vmem_schedule.read(&vmem_schedule, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t,latency_buffer), &latency_buffer, sizeof(latency_buffer));

                if ( (latency_buffer*1E9 >= (timestamp - time))  || (latency_buffer == 0) ) {
                    /* Read the whole schedule object */
                    int length = objstore_read_obj_length(&vmem_schedule, offset);
                    if (length < 0) {
                        continue;
                    }
                    param_schedule_t * temp_schedule = malloc((long int) length + sizeof(temp_schedule->queue.buffer));
                    void * read_ptr = (void*) ( (long int) temp_schedule + sizeof(temp_schedule->queue.buffer));
                    objstore_read_obj(&vmem_schedule, offset, read_ptr, 0);

                    temp_schedule->queue.buffer = (char *) ((long int) temp_schedule + (long int) (sizeof(param_schedule_t)));

                    /* Execute the scheduled queue */
                    if (param_push_queue(&temp_schedule->queue, 0, temp_schedule->host, 100) == 0){
                        temp_schedule->completed = 0x55;
                        temp_schedule->time = timestamp;
                    } else {
                        /* Postpone 10 seconds to retry in case of network errors */
                        temp_schedule->time += 10*1E9;
                    }

                    /* Write the results to objstore */
                    void * write_ptr = (void*) ( (long int) temp_schedule + sizeof(temp_schedule->queue.buffer));
                    objstore_write_obj(&vmem_schedule, offset, OBJ_TYPE_SCHEDULE, length, write_ptr);

                    free(temp_schedule);
                } else {
                    /* Latency buffer exceeded */
                    completed = 0xAA;
                    objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, completed), sizeof(completed), &completed);
                    objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, time), sizeof(time), &timestamp);
                }
            }
        } else {
            if (time <= (timestamp - (uint64_t) 86400*1E9)) {
                /* Deactivate completed schedule entries after 24 hrs */
                uint8_t active = 0;
                objstore_write_data(&vmem_schedule, offset, OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, active), sizeof(active), &active);
            }
        }
    }

    /* Delete inactive schedules */
    for (int i = 0; i < num_schedules; i++) {
        int offset = objstore_scan(&vmem_schedule, find_inactive_scancb, 0, NULL);
        if (offset < 0)
            break;

        objstore_rm_obj(&vmem_schedule, offset, 0);
    }

    //printf("Schedule server update completed.\n");
    return 0;
}

static void meta_obj_init(vmem_t * vmem) {
    /* Search for scheduler meta object */
    int offset = objstore_scan(vmem, find_meta_scancb, 0, NULL);
    
    if (offset < 0) {
        /* Not found, initialize default values and write the meta object */
        meta_obj.last_id = UINT16_MAX;
        offset = objstore_alloc(vmem, sizeof(meta_obj), 0);
        objstore_write_obj(vmem, offset, OBJ_TYPE_SCHEDULER_META, sizeof(meta_obj), (void *) &meta_obj);
    } else {
        if (objstore_read_obj(vmem, offset, (void *) &meta_obj, 0) < 0) {
            /* Invalid meta object checksum, reset to highest ID among active schedules */
            uint16_t max_id = 0;
            int num_schedules = get_number_of_schedule_objs(vmem);
            /* Check the time on each schedule and execute if deadline is exceeded */ 
            for (int i = 0; i < num_schedules; i++) {
                int obj_skips = i;
                int offset = objstore_scan(vmem, next_schedule_scancb, 0, (void *) &obj_skips);
                if (offset < 0) {
                    continue;
                }

                uint16_t read_id;
                vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_schedule_t, id), &read_id, sizeof(read_id));
                if (read_id > max_id)
                    max_id = read_id;
            }
            
            if (num_schedules <= 0) {
                meta_obj.last_id = UINT16_MAX;
            } else {
                meta_obj.last_id = max_id;
            }
            
            objstore_write_obj(vmem, offset, OBJ_TYPE_SCHEDULER_META, sizeof(meta_obj), (void *) &meta_obj);
        }
    }
}

void param_schedule_server_init(void) {
    vmem_file_init(&vmem_schedule);

    meta_obj_init(&vmem_schedule);
}