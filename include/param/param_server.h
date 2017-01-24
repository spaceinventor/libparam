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

csp_thread_return_t param_server_task(void *pvParameters);

typedef enum {
	PARAM_CHUNK_TIME,
	PARAM_CHUNK_NODE,
	PARAM_CHUNK_PARAM,
	PARAM_CHUNK_PARAMS,
	PARAM_CHUNK_PARAM_AND_VALUE,
	PARAM_CHUNK_TIME_AND_VALUE,
} param_server_chunks;

typedef enum {
	PARAM_PULL_REQUEST = 0,
	PARAM_PULL_RESPONSE = 1,
	PARAM_PUSH_REQUEST = 2,
	PARAM_PUSH_RESPONSE = 3,
} param_packet_type_e;

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t size;
	char name[];
} __attribute__((packed)) rparam_transfer_t;

void param_serve_pull_request(csp_conn_t * conn, csp_packet_t * request);
void param_serve_pull_response(csp_conn_t * conn, csp_packet_t * packet, int verbose);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_SERVER_H_ */
