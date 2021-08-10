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

typedef enum {
	SCHED_TIMER_TYPE_RELATIVE,
	SCHED_TIMER_TYPE_UNIX,
} param_timer_type_e;

struct param_schedule_s {
	param_queue_t queue;
    uint32_t time;
    param_timer_type_e timer_type;
    uint16_t id;
    uint16_t host;
    uint8_t completed;
    uint8_t active;
    csp_packet_t *buffer_ptr;
};

typedef struct param_schedule_s param_schedule_t;

int param_serve_schedule_push(csp_packet_t *packet);
int param_serve_schedule_pull(csp_packet_t *packet);
int param_serve_schedule_show(csp_packet_t *packet);
int param_serve_schedule_rm_single(csp_packet_t *packet);
int param_serve_schedule_rm_all(csp_packet_t *packet);
int param_serve_schedule_list(csp_packet_t *packet);

int param_schedule_server_update(uint32_t timestamp);
void param_schedule_server_init(void);