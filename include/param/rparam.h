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

	/* Parameter declaration */
	uint16_t id;
	param_type_e type;
	uint8_t size;
	char name[26]; // One extra to allow for '\0'

	/* Parameter definition */
	uint8_t node;
	uint16_t timeout;

	/* Used for rparam get/set */
	void * value_get;
	void * value_set;
	uint32_t value_updated; // Timestamp
	uint8_t value_pending; // 0 = none, 1 = pending, 2 = acked

	/* Used for linked list */
	struct rparam_t * next;

} __attribute__((packed)) rparam_t;

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t size;
	char name[];
} __attribute__((packed)) rparam_transfer_t;

#define _rparam_struct(_name, _node, _timeout, _id, _type, _size, _value_get, _value_set) \
	struct rparam_t _##_name = { \
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
