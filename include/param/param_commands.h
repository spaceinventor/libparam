/*
 * param_commands.h
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#pragma once

#include <param/param_queue.h>
#include <param/param_server.h>
#include <csp/csp.h>

extern vmem_t vmem_commands;

struct param_command_s {
	param_queue_t queue;
    uint16_t id;
    char name[14];
} __attribute__((packed, aligned(1)));
typedef struct param_command_s param_command_t;

struct param_command_buf_s {
    param_command_t header;
    char queue_buffer[PARAM_SERVER_MTU];
} __attribute__((packed, aligned(1)));
typedef struct param_command_buf_s param_command_buf_t;

struct param_commands_meta_s {
    uint16_t last_id;
};
typedef struct param_commands_meta_s param_commands_meta_t;

void param_command_server_init(void);

int param_serve_command_show(csp_packet_t *packet);
int param_serve_command_add(csp_packet_t *request);
int param_serve_command_list(csp_packet_t *request);
int param_serve_command_rm_single(csp_packet_t *packet);
int param_serve_command_rm_all(csp_packet_t *packet);

int param_command_read(char command_name[], param_command_buf_t * cmd_buffer);
