/*
 * rparam.h
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_
#define LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_

#include <csp/arch/csp_thread.h>
#include <param/param.h>

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
} param_packet_type_e;

/**
 * Parameter server task:
 *
 * A single instance should run on all nodes.
 *
 * @param pvParameters not used
 * @return task will never return
 */
csp_thread_return_t param_server_task(void *pvParameters);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_ */
