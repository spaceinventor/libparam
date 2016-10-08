/*
 * param_server.h
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_SERVER_H_
#define SRC_PARAM_PARAM_SERVER_H_

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

void param_server_task(void *pvParameters);

#endif /* SRC_PARAM_PARAM_SERVER_H_ */
