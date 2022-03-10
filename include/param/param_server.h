/*
 * rparam.h
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#pragma once 

#include <csp/csp_types.h>

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

/**
 * Handle incoming parameter requests
 *
 * These are stateless no task or csp connection needed
 *
 * @param packet
 */
void param_serve(csp_packet_t * packet);
