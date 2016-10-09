/*
 * rparam.h
 *
 *  Created on: Oct 8, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_RPARAM_H_
#define LIB_PARAM_INCLUDE_PARAM_RPARAM_H_

#include <csp/arch/csp_thread.h>
#include <param/param.h>

#define PARAM_SERVER_MTU 200
#define PARAM_PORT_GET	10
#define PARAM_PORT_SET	11

typedef struct {
	uint8_t len;
	uint8_t data[];
} __attribute__((packed)) param_response_t;

typedef struct {
	uint16_t id;
	uint64_t value;
} __attribute__((packed)) param_set_t;

csp_thread_return_t rparam_server_task(void *pvParameters);

typedef struct rparam_s {
	int node;
	int timeout;
	int idx;
	param_type_e type;
	char *name;
	int size;
	char *unit;
	uint64_t min;
	uint64_t max;
	param_readonly_type_e readonly;
} rparam_t;

#endif /* LIB_PARAM_INCLUDE_PARAM_RPARAM_H_ */
