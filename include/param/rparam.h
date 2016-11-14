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
	uint8_t node;
	uint16_t timeout;
	uint16_t id;
	param_type_e type;
	char name[26]; // One extra to allow for '\0'
	uint8_t size;
	char unit[5];
	uint64_t min;
	uint64_t max;
	param_readonly_type_e readonly;
	struct rparam_t * next;
	void * value;
	uint32_t value_updated; // Timestamp
	void * setvalue;
	uint8_t setvalue_pending; // 0 = OK, 1 = pending, 2 = acked
} __attribute__((packed)) rparam_t;

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t size;
	char name[];
} __attribute__((packed)) rparam_transfer_t;

#define RPARAM(_nodename, _node, _timeout, _id, _type, _name, _size, _unit, _readonly, _typecast) \
	_typecast _nodename##_##_name##_value; \
	_typecast _nodename##_##_name##_setvalue; \
	struct rparam_t _nodename##_##_name = { \
		.node = _node, \
		.timeout = _timeout, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.unit = _unit, \
		.readonly = _readonly, \
		.value = &_nodename##_##_name##_value, \
		.setvalue = &_nodename##_##_name##_setvalue, \
	}; \

int rparam_get(rparam_t * rparams[], int count, int verbose);
int rparam_set(rparam_t * rparams[], int count, int verbose);
int rparam_get_single(rparam_t * rparam);
int rparam_set_single(rparam_t * rparam);

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

#define RPARAM_SET(_type, _name) int rparam_set_##_name(rparam_t * rparam, _type value);
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

void rparam_print(rparam_t * rparam);
int rparam_size(rparam_t * rparam);

#endif /* LIB_PARAM_INCLUDE_PARAM_RPARAM_H_ */
