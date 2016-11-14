/*
 * param.h
 *
 *  Created on: Sep 21, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_H_
#define SRC_PARAM_PARAM_H_

#include <stdint.h>
#include <stdbool.h>
#include <vmem/vmem.h>

typedef enum {
	PARAM_TYPE_UINT8,
	PARAM_TYPE_UINT16,
	PARAM_TYPE_UINT32,
	PARAM_TYPE_UINT64,
	PARAM_TYPE_INT8,
	PARAM_TYPE_INT16,
	PARAM_TYPE_INT32,
	PARAM_TYPE_INT64,
	PARAM_TYPE_XINT8,
	PARAM_TYPE_XINT16,
	PARAM_TYPE_XINT32,
	PARAM_TYPE_XINT64,
	PARAM_TYPE_FLOAT,
	PARAM_TYPE_DOUBLE,
	PARAM_TYPE_STRING,
	PARAM_TYPE_DATA,
} param_type_e;

typedef enum {
	PARAM_READONLY_FALSE,         //! Not readonly
	PARAM_READONLY_TRUE,          //! Readonly Internal and external
	PARAM_READONLY_EXTERNAL,      //! Readonly External only
	PARAM_READONLY_INTERNAL,      //! Readonly Internal only
	PARAM_HIDDEN,                 //! Do not display on lists
} param_readonly_type_e;

typedef const struct param_s {
	uint16_t id;
	int addr;
	int size;
	param_type_e type;
	const char *name;
	const char *unit;
	const struct vmem_s * vmem;
	void * physaddr;
	uint64_t min;
	uint64_t max;
	param_readonly_type_e readonly;
	void (*callback)(const struct param_s * param);
} param_t;

extern param_t __start_param, __stop_param;

#ifndef PARAM_STORAGE_SIZE
static const param_t param_size_set[2] __attribute__((aligned(1)));
#define PARAM_STORAGE_SIZE ((intptr_t) &param_size_set[1] - (intptr_t) &param_size_set[0])
#endif

#define param_foreach(_c) \
	for (_c = &__start_param; \
	     _c < &__stop_param; \
	     _c = (param_t *)(intptr_t)((char *)_c + PARAM_STORAGE_SIZE))

#define PARAM_DEFINE_STATIC_RAM(_id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _physaddr) \
	__attribute__((section("param"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.min = _min, \
		.max = _max, \
		.readonly = _readonly, \
		.unit = _unit, \
		.callback = _callback, \
		.physaddr = _physaddr, \
	}

#define PARAM_DEFINE_STRUCT_RAM(fieldname, _id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _physaddr) \
	.fieldname = { \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.min = _min, \
		.max = _max, \
		.readonly = _readonly, \
		.unit = _unit, \
		.callback = _callback, \
		.physaddr = _physaddr, \
	}


#define PARAM_DEFINE_STATIC_VMEM(_id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _vmem_name, _addr) \
	__attribute__((section("param"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.min = _min, \
		.max = _max, \
		.readonly = _readonly, \
		.callback = _callback, \
		.unit = _unit, \
		.addr = _addr, \
		.vmem = &vmem_##_vmem_name##_instance, \
	}

#define PARAM_DEFINE_STRUCT_VMEM(fieldname, _id, _type, _size, _min, _max, _readonly, _callback, _unit, _vmem_name, _addr) \
	.fieldname = { \
		.id = _id, \
		.type = _type, \
		.name = #_vmem_name "_" #fieldname, \
		.size = _size, \
		.min = _min, \
		.max = _max, \
		.readonly = _readonly, \
		.callback = _callback, \
		.unit = _unit, \
		.addr = _addr, \
		.vmem = &vmem_##_vmem_name##_instance, \
	}

void param_print(param_t * param);
void param_list(char * token);
void param_list_array(param_t * param, int count);
void param_value_str(param_t *param, char * out, int len);

#define PARAM_GET(type, name) \
	type param_get_##name(param_t * param);
PARAM_GET(uint8_t, uint8)
PARAM_GET(uint16_t, uint16)
PARAM_GET(uint32_t, uint32)
PARAM_GET(uint64_t, uint64)
PARAM_GET(int8_t, int8)
PARAM_GET(int16_t, int16)
PARAM_GET(int32_t, int32)
PARAM_GET(int64_t, int64)
PARAM_GET(float, float)
PARAM_GET(double, double)
#undef PARAM_GET

#define PARAM_SET(type, name) \
	void param_set_##name(param_t * param, type value); \
	void param_set_##name##_nocallback(param_t * param, type value);
PARAM_SET(uint8_t, uint8)
PARAM_SET(uint16_t, uint16)
PARAM_SET(uint32_t, uint32)
PARAM_SET(uint64_t, uint64)
PARAM_SET(int8_t, int8)
PARAM_SET(int16_t, int16)
PARAM_SET(int32_t, int32)
PARAM_SET(int64_t, int64)
PARAM_SET(float, float)
PARAM_SET(double, double)
#undef PARAM_SET

void param_set(param_t * param, void * value);
void param_set_data(param_t * param, void * inbuf, int len);
void param_get_data(param_t * param, void * outbuf, int len);
#define param_set_string param_set_data
#define param_get_string param_get_data

param_t * param_ptr_from_id(int id);
param_t * param_ptr_from_idx(int idx);
int param_idx_from_ptr(param_t * param);

param_t * param_name_to_ptr(char * name);

int param_typesize(param_type_e type);

#endif /* SRC_PARAM_PARAM_H_ */
