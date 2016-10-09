/*
 * rparam.h
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_RPARAM_H_
#define LIB_PARAM_INCLUDE_PARAM_RPARAM_H_

#include <csp/arch/csp_thread.h>

#define PARAM_SERVER_MTU 200
#define PARAM_PORT_GET	10
#define PARAM_PORT_SET	11

typedef struct {
	uint16_t id;
} __attribute__((packed)) param_request_t;

typedef struct {
	uint8_t len;
	uint8_t data[];
} __attribute__((packed)) param_response_t;

typedef struct {
	uint16_t id;
	uint64_t value;
} __attribute__((packed)) param_set_t;

csp_thread_return_t rparam_server_task(void *pvParameters);


#endif /* LIB_PARAM_INCLUDE_PARAM_RPARAM_H_ */
