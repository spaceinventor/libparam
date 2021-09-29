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
    
    objstore_write_obj(vmem, offset, OBJ_TYPE_COMMANDS_META, sizeof(meta_obj), (void *) &meta_obj);
}

static int find_name_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int type = objstore_read_obj_type(vmem, offset);

    if (type != OBJ_TYPE_COMMAND)
        return 0;

    int name_length = strlen((char *)ctx);

    char data;
    int status = 0;
    for (int i = 0; i < name_length; i++) {
        vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_command_t, name)+i, &data, sizeof(data));

        if ( ((char *)ctx)[i] == data) {
            status++;
        } else {
            break;
        }
    }

    if (status == name_length)
        return -1;

    return 0;
}

static int obj_offset_from_name(vmem_t * vmem, char name[]) {
    if ( (strlen(name) == 0) || (strlen(name) > 13))
        return -1;
    
    int offset = objstore_scan(vmem, find_name_scancb, 0, name);
    
    return offset;
}

static void name_copy(char output[], char input[], int length) {
    for (int i = 0; i < length; i++) {
			output[i] = input[i];
    }
    output[length] = '\0';
}

static uint16_t command_add(csp_packet_t * request, param_queue_type_e q_type) {
    if (request->length < (3 + request->data[2]))
        return UINT16_MAX;
    
    int queue_size = request->length - 3 - request->data[2];

    char tmp_name[14];
    int name_length = request->data[2];

    name_copy(tmp_name, (char *) &request->data[3], name_length);

    /* Check if a command with this name already exists and remove it if it does */
    int old_offset = obj_offset_from_name(&vmem_commands, tmp_name);
    if (old_offset >= 0) {
        objstore_rm_obj(&vmem_commands, old_offset, 0);
    }

    /* Determine command size and allocate VMEM */
    int obj_length = (int) sizeof(param_command_t) + queue_size - (int) sizeof(char *);

    int obj_offset = objstore_alloc(&vmem_commands, obj_length, 0);
    if (obj_offset < 0)
        return UINT16_MAX;

    param_command_t * temp_command = malloc(sizeof(param_command_t) + queue_size);

    name_copy(temp_command->name, (char *) &request->data[3], name_length);

    int meta_offset = objstore_scan(&vmem_commands, find_meta_scancb, 0, NULL);
    if (meta_offset < 0) {
        free(temp_command);
        return UINT16_MAX;
    }

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

int param_serve_command_add(csp_packet_t * request) {
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

/*static int obj_offset_from_id_scancb(vmem_t * vmem, int offset, int verbose, void * ctx) {
    int target_id =  *(int*)ctx;

    int type = objstore_read_obj_type(vmem, offset);
    if (type != OBJ_TYPE_COMMAND)
        return 0;

    uint16_t id;
    vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_command_t,id), &id, sizeof(id));
    
    if (id == target_id) {
        return -1;
    }

    return 0;
}

static int obj_offset_from_id(vmem_t * vmem, int id) {
    int offset = objstore_scan(vmem, obj_offset_from_id_scancb, 0, (void*) &id);
    return offset;
}*/

int param_serve_command_show(csp_packet_t *packet) {
    char name[14];
    int name_length = packet->data[2];
    name_copy(name, (char *) &packet->data[3], name_length);

    
    int status = 0;
    int offset = obj_offset_from_name(&vmem_commands, name);
    if (offset < 0) {
        status = -1;
    }
    int length = objstore_read_obj_length(&vmem_commands, offset);
    if (length < 0) {
        status = -1;
    }

    if (status == 0) {
        /* Read the command entry */
        param_command_t * temp_command = malloc(length + sizeof(temp_command->queue.buffer));
        void * read_ptr = (void*) ( (long int) temp_command + sizeof(temp_command->queue.buffer));
        objstore_read_obj(&vmem_commands, offset, read_ptr, 0);

        temp_command->queue.buffer = (char *) ((long int) temp_command + (long int) (sizeof(param_command_t)));

        /* Respond with the requested command entry */
        packet->data[0] = PARAM_COMMAND_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data[2] = (uint8_t) name_length;
        packet->data[3] = (uint8_t) temp_command->queue.type;

        memcpy(&packet->data[4], temp_command->name, name_length);
        
        memcpy(&packet->data[4+name_length], temp_command->queue.buffer, temp_command->queue.used);

        packet->length = 4 + name_length + temp_command->queue.used;

        free(temp_command);
    } else {
        /* Respond with an error code */
        packet->data[0] = PARAM_COMMAND_SHOW_RESPONSE;
        packet->data[1] = PARAM_FLAG_END;

        packet->data[2] = UINT8_MAX;

        packet->length = 3;
    }

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
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

int param_serve_command_list(csp_packet_t *request) {
    int num_commands = get_number_of_command_objs(&vmem_commands);
    unsigned int counter = 0;
    unsigned int big_count = 0;
    int end = 0;

    while (end == 0) {
        csp_packet_t * response = csp_buffer_get(PARAM_SERVER_MTU);
        if (response == NULL)
            break;

        char name[14];
        int num_per_packet = (PARAM_SERVER_MTU-4)/14;
        
        if ( (num_commands - big_count*num_per_packet)*14 < (PARAM_SERVER_MTU - 4))
            end = 1;

        int loop_max = (big_count+1)*num_per_packet;
        if (end == 1)
            loop_max = num_commands;

        for (int i = big_count*num_per_packet; i < loop_max; i++) {
            int obj_skips = counter;
            int offset = objstore_scan(&vmem_commands, next_command_scancb, 0, (void *) &obj_skips);
            if (offset < 0) {
                counter++;
                continue;
            }

            vmem_commands.read(&vmem_commands, offset + OBJ_HEADER_LENGTH - sizeof(char *) + offsetof(param_command_t, name), &name, 14);

            unsigned int idx = 4+(counter-big_count*num_per_packet)*14;

            memcpy(&response->data[idx], name, 14);
            
            counter++;
        }

        response->data[0] = PARAM_COMMAND_LIST_RESPONSE;
        if (end == 1) {
            response->data[1] = PARAM_FLAG_END;
        } else {
            response->data[1] = 0;
        }
        response->data16[1] = csp_hton16(counter-big_count*num_per_packet); // number of entries
        response->length = (counter-big_count*num_per_packet)*14 + 4;

        if (csp_sendto_reply(request, response, CSP_O_SAME, 0) != CSP_ERR_NONE)
            csp_buffer_free(response);

        big_count++;
    }

    csp_buffer_free(request);

    return 0;
}

int param_serve_command_rm_single(csp_packet_t *packet) {
    /* Erase the specified command */
    char name[14];
    int name_length = packet->data[2];
    name_copy(name, (char *) &packet->data[3], name_length);

    int offset = obj_offset_from_name(&vmem_commands, name);
    if (offset < 0) {
        return -1;
        csp_buffer_free(packet);
    }

    if (objstore_rm_obj(&vmem_commands, offset, 0) < 0) {
        return -1;
        csp_buffer_free(packet);
    }
    
    /* Respond with the name again to verify which command was erased */
	packet->data[0] = PARAM_COMMAND_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = csp_hton16(strlen(name));
    
    memcpy(&packet->data[4], name, strlen(name) + 1);
	packet->length = 4 + strlen(name) + 1;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

int param_serve_command_rm_all(csp_packet_t *packet) {
    /* Confirm remove all by checking that name = "RMALLCMDS" */
    char name[14];
    int name_length = packet->data[2];
    if (name_length != 9) {
        return -1;
        csp_buffer_free(packet);
    }
    name_copy(name, (char *) &packet->data[3], name_length);
    char rmallcmds[] = "RMALLCMDS";
    for (int i = 0; i < strlen(rmallcmds); i++) {
        if (name[i] != rmallcmds[i]) {
            return -1;
            csp_buffer_free(packet);
        }
    }

    int num_commands = get_number_of_command_objs(&vmem_commands);
    uint16_t deleted_commands = 0;
    /* Iterate over the number of commands and erase each */
    for (int i = 0; i < num_commands; i++) {
        int obj_skips = 0;
        int offset = objstore_scan(&vmem_commands, next_command_scancb, 0, (void *) &obj_skips);
        if (offset < 0) {
            continue;
        }

        if (objstore_rm_obj(&vmem_commands, offset, 0) < 0) {
            continue;
        }
        deleted_commands++;
    }

    /** Respond with name = RMALLCMDS again to verify that all commands have been erased
     * include the number of commands deleted in the response */
	packet->data[0] = PARAM_COMMAND_RM_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
    packet->data16[1] = csp_hton16(deleted_commands);

    memcpy(&packet->data[4], rmallcmds, 10);

	packet->length = 14;

	if (csp_sendto_reply(packet, packet, CSP_O_SAME, 0) != CSP_ERR_NONE)
		csp_buffer_free(packet);

    return 0;
}

/* Remember to free the returned pointer after use */
param_command_t * param_command_read(char command_name[]) {
    int offset = obj_offset_from_name(&vmem_commands, command_name);
    if (offset < 0) {
        return NULL;
    }
    int length = objstore_read_obj_length(&vmem_commands, offset);
    if (length < 0) {
        return NULL;
    }

    /* Read the command entry */
    param_command_t * temp_command = malloc(length + sizeof(temp_command->queue.buffer));
    void * read_ptr = (void*) ( (long int) temp_command + sizeof(temp_command->queue.buffer));
    objstore_read_obj(&vmem_commands, offset, read_ptr, 0);

    temp_command->queue.buffer = (char *) ((long int) temp_command + (long int) (sizeof(param_command_t)));

    return temp_command;
}

static void meta_obj_init(vmem_t * vmem) {
    /* Search for commands meta object */
    int offset = objstore_scan(vmem, find_meta_scancb, 0, NULL);
    
    if (offset < 0) {
        /* Not found, initialize default values and write the meta object */
        meta_obj.last_id = UINT16_MAX;
        offset = objstore_alloc(vmem, sizeof(meta_obj), 0);
        objstore_write_obj(vmem, offset, OBJ_TYPE_COMMANDS_META, sizeof(meta_obj), (void *) &meta_obj);
    } else {
        if (objstore_read_obj(vmem, offset, (void *) &meta_obj, 0) < 0) {
            /* Invalid meta object checksum, reset to highest ID among active commands */
            uint16_t max_id = 0;
            int num_commands = get_number_of_command_objs(vmem);
            /* Check the time on each command and execute if deadline is exceeded */ 
            for (int i = 0; i < num_commands; i++) {
                int obj_skips = i;
                int offset = objstore_scan(vmem, next_command_scancb, 0, (void *) &obj_skips);
                if (offset < 0) {
                    continue;
                }

                uint16_t read_id;
                vmem->read(vmem, offset+OBJ_HEADER_LENGTH-sizeof(char*)+offsetof(param_command_t, id), &read_id, sizeof(read_id));
                if (read_id > max_id)
                    max_id = read_id;
            }
            
            if (num_commands <= 0) {
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