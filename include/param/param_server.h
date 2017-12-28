/*
 * rparam.h
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_
#define LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_

#include <csp/arch/csp_thread.h>

#define PARAM_SERVER_MTU 200
#define PARAM_PORT_SERVER 10
#define PARAM_PORT_LIST	12

/**
 * First byte on all packets is the packet type
 */
typedef enum {
	PARAM_PULL_REQUEST 	= 0,
	PARAM_PULL_RESPONSE = 1,
	PARAM_PUSH_REQUEST 	= 2,
	PARAM_PUSH_RESPONSE = 3,
	PARAM_PULL_ALL_REQUEST = 4,
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

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_ */
