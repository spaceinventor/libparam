/*
 * param_scheduler.h
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#pragma once

#include <param/param_queue.h>
#include <csp/csp.h>

#define SCHEDULE_LIST_LENGTH 32

struct param_schedule_s {
	param_queue_t queue;
    uint32_t time;
    uint16_t id;
    uint8_t completed;
    uint8_t active;
};

typedef struct param_schedule_s param_schedule_t;

int param_serve_schedule_push(csp_packet_t *packet);
int param_serve_schedule_show(csp_packet_t *packet);