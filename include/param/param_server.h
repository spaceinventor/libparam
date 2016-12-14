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
#define PARAM_PORT_GET	10
#define PARAM_PORT_SET	11
#define PARAM_PORT_LIST	12
csp_thread_return_t param_server_task(void *pvParameters);

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t size;
	char name[];
} __attribute__((packed)) rparam_transfer_t;

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_ */
