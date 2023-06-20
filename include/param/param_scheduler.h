/*
 * param_scheduler.h
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#pragma once

#include <param/param_queue.h>
#include <param/param_server.h>
#include <csp/csp.h>

extern vmem_t vmem_schedule;

struct param_schedule_s {
	param_queue_t queue;
    uint64_t time __attribute__((aligned(8)));
    uint32_t latency_buffer;
    uint16_t id;
    uint16_t host;
    uint8_t completed;
    uint8_t active;
    uint8_t retries;
    // manual alignment
    uint8_t reserved_1;
} __attribute__((packed, aligned(1)));
typedef struct param_schedule_s param_schedule_t;

struct param_schedule_buf_s {
    param_schedule_t header;
    char queue_buffer[PARAM_SERVER_MTU] __attribute__((aligned(1)));
} __attribute__((packed, aligned(8)));
typedef struct param_schedule_buf_s param_schedule_buf_t;

struct param_scheduler_meta_s {
    uint16_t last_id;
} __attribute__((packed, aligned(2)));
typedef struct param_scheduler_meta_s param_scheduler_meta_t;

int param_serve_schedule_push(csp_packet_t *packet);
int param_serve_schedule_pull(csp_packet_t *packet);
int param_serve_schedule_show(csp_packet_t *packet);
int param_serve_schedule_rm_single(csp_packet_t *packet);
int param_serve_schedule_rm_all(csp_packet_t *packet);
int param_serve_schedule_list(csp_packet_t *packet);
void param_serve_schedule_reset(csp_packet_t *packet);

/**
 * @brief Check and execute current scheduled tasks whose timestamp has been exceeded.
 * 
 * @param timestamp_nsec Timestamp to compare scheduled tasks against, ought to be the current system timestamp.
 * @return int 0 when scheduled tasks were successfully checked or executed.
 */
int param_schedule_server_update(uint64_t timestamp_nsec);
void param_schedule_server_init(void);


#ifdef PARAM_HAVE_COMMANDS
int param_serve_schedule_command(csp_packet_t *request);
#endif
