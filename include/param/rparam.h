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

#define _rparam_struct(_name, _node, _timeout, _id, _type, _size, _value_get, _value_set) \
	struct param_s _##_name = { \
		.paramtype = 1, \
		.id = _id, \
		.type = _type, \
		.size = _size, \
		.name = #_name, \
		\
		.node = _node, \
		.timeout = _timeout, \
		\
		.value_get = _value_get, \
		.value_set = _value_set \
	};

#define rparam_define_readonly(_name, _node, _timeout, _id, _type, _size) \
	char __attribute__((aligned(8))) _##_name##_value_get[_size]; \
	_rparam_struct(_name, _node, _timeout, _id, _type, _size, _##_name##_value_get, NULL)


#define rparam_define_readwrite(_name, _node, _timeout, _id, _type, _size) \
	char __attribute__((aligned(8))) _##_name##_value_get[_size]; \
	char __attribute__((aligned(8))) _##_name##_value_set[_size]; \
	_rparam_struct(_name, _node, _timeout, _id, _type, _size, _##_name##_value_get, _##name##_value_set)

int rparam_get(param_t * rparams[], int count, int verbose);
int rparam_set(param_t * rparams[], int count, int verbose);
int rparam_get_single(param_t * rparam);
int rparam_set_single(param_t * rparam);

#define RPARAM_GET(_type, _name) _type rparam_get_##_name(param_t * rparam);
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

#define RPARAM_SET(_type, _name) int rparam_set_##_name(param_t * rparam, _type value);
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

void rparam_print(param_t * rparam);
int rparam_size(param_t * rparam);

#endif /* LIB_PARAM_INCLUDE_PARAM_RPARAM_H_ */
