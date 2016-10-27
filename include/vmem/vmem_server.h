/*
 * vmem_server.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>

#define VMEM_SERVER_TIMEOUT 30000
#define VMEM_SERVER_MTU 192
#define VMEM_PORT_SERVER 13

#define VMEM_MAX(a,b) ((a) > (b) ? a : b)
#define VMEM_MIN(a,b) ((a) < (b) ? a : b)

typedef enum {
	VMEM_SERVER_UPLOAD,
	VMEM_SERVER_DOWNLOAD,
} vmem_request_type;

typedef struct {
	uint8_t version;
	uint8_t type;
	uint32_t address;
	uint32_t length;
} __attribute__((packed)) vmem_request_t;

void vmem_server_handler(csp_conn_t * conn);
csp_thread_return_t vmem_server_task(void *pvParameters);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_ */
