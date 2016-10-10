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

typedef struct rparam_t {
	int node;
	int timeout;
	int idx;
	param_type_e type;
	char name[13];
	int size;
	char unit[5];
	uint64_t min;
	uint64_t max;
	param_readonly_type_e readonly;
	struct rparam_t * next;
} rparam_t;

typedef struct {
	uint16_t idx;
	uint8_t type;
	char name[13];
} __attribute__((packed)) rparam_transfer_t;

int rparam_get(rparam_t * rparam, void * out);
int rparam_set(rparam_t * rparam, void * in);

#define RPARAM_GET(_type, _name) _type rparam_get_##_name(rparam_t * rparam);
RPARAM_GET(uint8_t, uint8)
RPARAM_GET(uint16_t, uint16)
RPARAM_GET(uint32_t, uint32)
RPARAM_GET(uint64_t, uint64)
RPARAM_GET(int8_t, int8)
RPARAM_GET(int16_t, int16)
RPARAM_GET(int32_t, int32)
RPARAM_GET(int64_t, int64)
RPARAM_GET(float, float)
RPARAM_GET(double, double)
#undef RPARAM_GET

#define RPARAM_SET(_type, _name) void rparam_set_##_name(rparam_t * rparam, _type value);
RPARAM_SET(uint8_t, uint8)
RPARAM_SET(uint16_t, uint16)
RPARAM_SET(uint32_t, uint32)
RPARAM_SET(uint64_t, uint64)
RPARAM_SET(int8_t, int8)
RPARAM_SET(int16_t, int16)
RPARAM_SET(int32_t, int32)
RPARAM_SET(int64_t, int64)
RPARAM_SET(float, float)
RPARAM_SET(double, double)
#undef RPARAM_SET

#endif /* LIB_PARAM_INCLUDE_PARAM_RPARAM_H_ */
