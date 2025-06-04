/*
 * rparam.h
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#pragma once 

#include <csp/csp_types.h>
#include <param/param.h>

#define PARAM_SERVER_MTU 200
#define PARAM_PORT_SERVER 10
#define PARAM_PORT_LIST	12

/**
 * First byte on all packets is the packet type
 */
typedef enum {

    /* V1 */
	PARAM_PULL_REQUEST 	= 0,
	PARAM_PULL_RESPONSE = 1,
	PARAM_PUSH_REQUEST 	= 2,
	PARAM_PUSH_RESPONSE = 3,        // Push response is an ack, and is same for v1 and v2.
	PARAM_PULL_ALL_REQUEST = 4,     // Pull all request is same for v1 and v2.

	/* V2 */
	PARAM_PULL_REQUEST_V2  = 5,
    PARAM_PULL_RESPONSE_V2 = 6,
    PARAM_PUSH_REQUEST_V2  = 7,
    PARAM_PULL_ALL_REQUEST_V2 = 8,
	PARAM_SCHEDULE_PUSH = 9,
	//PARAM_SCHEDULE_PULL = 10,
	PARAM_SCHEDULE_ADD_RESPONSE = 11, // same response for adding push or pull schedule
	PARAM_SCHEDULE_SHOW_REQUEST = 12,
	PARAM_SCHEDULE_SHOW_RESPONSE = 13,
	PARAM_SCHEDULE_LIST_REQUEST = 14,
	PARAM_SCHEDULE_LIST_RESPONSE = 15,
	PARAM_SCHEDULE_RM_ALL_REQUEST = 16,
	PARAM_SCHEDULE_RM_REQUEST = 17,
	PARAM_SCHEDULE_RM_RESPONSE = 18,
	PARAM_SCHEDULE_RESET_REQUEST = 19,
	PARAM_SCHEDULE_RESET_RESPONSE = 20,
	PARAM_COMMAND_ADD_REQUEST = 21,
	PARAM_COMMAND_ADD_RESPONSE = 22,
	PARAM_COMMAND_SHOW_REQUEST = 23,
	PARAM_COMMAND_SHOW_RESPONSE = 24,
	PARAM_COMMAND_LIST_REQUEST = 25,
	PARAM_COMMAND_LIST_RESPONSE = 26,
	PARAM_COMMAND_RM_REQUEST = 27,
	PARAM_COMMAND_RM_ALL_REQUEST = 28,
	PARAM_COMMAND_RM_RESPONSE = 29,
	PARAM_COMMAND_EXEC_REQUEST = 30,
	PARAM_COMMAND_EXEC_RESPONSE = 31,
	PARAM_SCHEDULE_COMMAND_REQUEST = 32,
	PARAM_PUSH_REQUEST_V2_HWID = 33,

} param_packet_type_e;

/**
 * Second byte on all packets contains flags
 */
#define PARAM_FLAG_END (1 << 7)
#define PARAM_FLAG_NOACK (1 << 6)
#define PARAM_FLAG_PULLWITHACK (1 << 0)

/**
 * Handle incoming parameter requests
 *
 * These are stateless no task or csp connection needed
 *
 * @param packet
 */
void param_serve(csp_packet_t * packet);

#if PARAM_NUM_PUBLISHQUEUES > 4
#error A maximum number of four param queues are supported
#endif

#if PARAM_NUM_PUBLISHQUEUES > 0
typedef struct param_publish_s {
	param_t * param;
	uint32_t queue;
} param_publish_t;

typedef enum {
	PARAM_PUBLISHQUEUE_0 = 0,
#if PARAM_NUM_PUBLISHQUEUES >= 2
	PARAM_PUBLISHQUEUE_1 = 1,
#endif
#if PARAM_NUM_PUBLISHQUEUES >= 3
	PARAM_PUBLISHQUEUE_2 = 2,
#endif
#if PARAM_NUM_PUBLISHQUEUES >= 4
	PARAM_PUBLISHQUEUE_3 = 3,
#endif
} param_publish_id_t;

#define PARAM_ADD_PUBLISH(paramname, queueid) \
param_publish_t __param_publish_##paramname##queueid = { \
	.param = &paramname, \
	.queue = queueid, \
}; \
__attribute__((section("param_publish"))) \
param_publish_t const * _param_publish_##paramname##queueid = & __param_publish_##paramname##queueid;

typedef bool (*param_shall_publish_t)(uint8_t queue);

void param_publish_periodic();
void param_publish_configure(param_publish_id_t queueid, uint16_t destination, uint16_t periodicity_ms, csp_prio_t csp_prio);
void param_publish_init(param_shall_publish_t criteria_cb);

#endif
