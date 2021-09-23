/*
 * param_commands.h
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#pragma once

#include <param/param_queue.h>
#include <csp/csp.h>

struct param_command_s {
	param_queue_t queue;
    uint16_t id;
    char name[14];
} __attribute__((packed, aligned(1)));
typedef struct param_command_s param_command_t;

struct param_commands_meta_s {
    uint16_t last_id;
};
typedef struct param_commands_meta_s param_commands_meta_t;

