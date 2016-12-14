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
#define PARAM_PORT_LIST	12
csp_thread_return_t rparam_server_task(void *pvParameters);

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t size;
	char name[];
} __attribute__((packed)) rparam_transfer_t;

int rparam_get(param_t * rparams[], int count, int verbose);
int rparam_set(param_t * rparams[], int count, int verbose);
int rparam_get_single(param_t * rparam);
int rparam_set_single(param_t * rparam);

void rparam_print(param_t * rparam);
int rparam_size(param_t * rparam);

#endif /* LIB_PARAM_INCLUDE_PARAM_RPARAM_H_ */
