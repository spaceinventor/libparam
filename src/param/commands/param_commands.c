/*
 * param_commands.c
 *
 *  Created on: Sep 23, 2021
 *      Author: Mads
 */

#include <param/param_commands.h>

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

VMEM_DEFINE_FILE(commands, "commands", "commands.cnf", 0x1000);

param_commands_meta_t meta_obj;


static int find_meta_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);

    if (type == OBJ_TYPE_COMMANDS_META)
        return -1;

    return 0;
}

static void meta_obj_save(vmem_t * vmem) {
    int offset = objstore_scan(vmem, find_meta_scancb, 0, NULL);
    if (offset < 0)
        return;
    
    objstore_write_obj(vmem, offset, OBJ_TYPE_SCHEDULER_META, sizeof(meta_obj), (void *) &meta_obj);
}

static uint16_t command_add(csp_packet_t * request, param_queue_type_e q_type) {
    if (request->length < (3 + request->data[2]))
        return UINT16_MAX;
    
    int queue_size = request->length - 3 - request->data[2];

    /* Determine schedule size and allocate VMEM */
    int obj_length = (int) sizeof(param_command_t) + queue_size - (int) sizeof(char *);
    if (obj_length < 0)
        return UINT16_MAX;

    int obj_offset = objstore_alloc(&vmem_commands, obj_length, 0);
    if (obj_offset < 0)
        return UINT16_MAX;

    param_command_t * temp_command = malloc(sizeof(param_command_t) + queue_size);

    int name_length = request->data[2];

    for (int i = 0; i < name_length; i++) {
			temp_command->name[i] = request->data[i+3];
    }
    temp_command->name[name_length] = '\0';

    int meta_offset = objstore_scan(&vmem_commands, find_meta_scancb, 0, NULL);
    if (meta_offset < 0)
        return UINT16_MAX;

    if (objstore_read_obj(&vmem_commands, meta_offset, (void *) &meta_obj, 0) < 0)
        printf("Meta obj checksum error\n");
    meta_obj.last_id++;
    if (meta_obj.last_id == UINT16_MAX){
        meta_obj.last_id = 0;
    }
    meta_obj_save(&vmem_commands);
    temp_command->id = meta_obj.last_id;
    
    /* Initialize command queue and copy queue buffer from CSP packet */
    param_queue_init(&temp_command->queue, (char *) temp_command + sizeof(param_command_t), queue_size, queue_size, q_type, 2);
    memcpy(temp_command->queue.buffer, &request->data[3+name_length], temp_command->queue.used);

    void * write_ptr = (void *) (long int) temp_command + sizeof(temp_command->queue.buffer);
    objstore_write_obj(&vmem_commands, obj_offset, (uint8_t) OBJ_TYPE_COMMAND, (uint8_t) obj_length, write_ptr);
    
    free(temp_command);
    return meta_obj.last_id;
}

void param_serve_command_push(csp_packet_t * request) {
    csp_packet_t * response = csp_buffer_get(0);
    if (response == NULL) {
        csp_buffer_free(request);
        return -1;
    }

    uint16_t id = command_add(request, PARAM_QUEUE_TYPE_SET);

    /* Send ack with ID */
	response->data[0] = PARAM_COMMAND_ADD_RESPONSE;
	response->data[1] = PARAM_FLAG_END;
    response->data16[1] = csp_hton16(id);
	response->length = 4;

	if (csp_sendto_reply(request, response, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(response);

    csp_buffer_free(request);

    return 0;
}


static int obj_offset_from_id_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int target_id =  *(int*)ctx;

    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_COMMAND)
        return 0;

    uint16_t id;
    vmem->read(vmem, offset+7-sizeof(char*)+offsetof(param_command_t,id), &id, sizeof(id));
    
    if (id == target_id) {
        return -1;
    }

    return 0;
}

static int obj_offset_from_id(vmem_t * vmem, int id) {
    int offset = objstore_scan(vmem, obj_offset_from_id_scancb, 0, (void*) &id);
    return offset;
}

static int num_commands_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_COMMAND)
        return 0;

    /* Found a command object, increment ctx */
    *(int*)ctx += 1;

    return 0;
}

static int get_number_of_command_objs(vmem_t * vmem) {
    int num_command_objs = 0;

    objstore_scan(vmem, num_commands_scancb, 0, (void *) &num_command_objs);

    return num_command_objs;
}

static int next_command_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_COMMAND)
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
            int num_schedules = get_number_of_command_objs(vmem);
            /* Check the time on each schedule and execute if deadline is exceeded */ 
            for (int i = 0; i < num_schedules; i++) {
                int obj_skips = i;
                int offset = objstore_scan(vmem, next_command_scancb, 0, (void *) &obj_skips);
                if (offset < 0) {
                    continue;
                }

                uint16_t read_id;
                vmem->read(vmem, offset+7-sizeof(char*)+offsetof(param_command_t, id), &read_id, sizeof(read_id));
                if (read_id > max_id)
                    max_id = read_id;
            }
            
            if (num_schedules <= 0) {
                meta_obj.last_id = UINT16_MAX;
            } else {
                meta_obj.last_id = max_id;
            }
            
            objstore_write_obj(vmem, offset, OBJ_TYPE_COMMANDS_META, sizeof(meta_obj), (void *) &meta_obj);
        }
    }
}

void param_command_server_init(void) {
    vmem_file_init(&vmem_commands);

    meta_obj_init(&vmem_commands);
}