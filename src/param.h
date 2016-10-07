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
#include <vmem.h>

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
} param_readonly_type_e;

typedef struct param_s {
	int addr;
	int size;
	param_type_e type;
	const char *name;
	const char *unit;
	struct vmem_s * vmem;
	void * physaddr;
	uint64_t min;
	uint64_t max;
	param_readonly_type_e readonly;
	void (*callback)(struct param_s * param);
} param_t;

#define PARAM_DEFINE_STATIC_RAM(name_in, type_in, size_in, min_in, max_in, readonly_in, callback_in, unit_in, physaddr_in) \
	__attribute__ ((section("param"), used)) \
	param_t name_in = { \
		.type = type_in, \
		.name = #name_in, \
		.size = size_in, \
		.min = min_in, \
		.max = max_in, \
		.readonly = readonly_in, \
		.unit = unit_in, \
		.callback = callback_in, \
		.physaddr = physaddr_in, \
	}

#define PARAM_DEFINE_STRUCT_RAM(fieldname, name_in, type_in, size_in, min_in, max_in, readonly_in, callback_in, unit_in, physaddr_in) \
	.fieldname = { \
		.type = type_in, \
		.name = #name_in, \
		.size = size_in, \
		.min = min_in, \
		.max = max_in, \
		.readonly = readonly_in, \
		.unit = unit_in, \
		.callback = callback_in, \
		.physaddr = physaddr_in, \
	}


#define PARAM_DEFINE_STATIC_VMEM(_name, type_in, size_in, min_in, max_in, readonly_in, callback_in, unit_in, vmem_name, addr_in) \
	__attribute__ ((section("param"), used)) \
	param_t _name = { \
		.type = type_in, \
		.name = #_name, \
		.size = size_in, \
		.min = min_in, \
		.max = max_in, \
		.readonly = readonly_in, \
		.callback = callback_in, \
		.unit = unit_in, \
		.addr = addr_in, \
		.vmem = &vmem_##vmem_name##_instance, \
	}

#define PARAM_DEFINE_STRUCT_VMEM(fieldname, type_in, size_in, min_in, max_in, readonly_in, callback_in, unit_in, vmem_name, addr_in) \
	.fieldname = { \
		.type = type_in, \
		.name = #vmem_name "_" #fieldname, \
		.size = size_in, \
		.min = min_in, \
		.max = max_in, \
		.readonly = readonly_in, \
		.callback = callback_in, \
		.unit = unit_in, \
		.addr = addr_in, \
		.vmem = &vmem_##vmem_name##_instance, \
	}


void param_callback_enabled(bool callbacks_enabled);
param_t * param_from_id(uint16_t id);
void param_print(param_t * param);
void param_list(struct vmem_s * vmem);
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
	void param_set_##name(param_t * param, type value);
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

void param_set_data(param_t * param, void * inbuf, int len);
void param_get_data(param_t * param, void * outbuf, int len);
#define param_set_string param_set_data
#define param_get_string param_get_data

param_t * param_index_to_ptr(int idx);
int param_ptr_to_index(param_t * param);

#endif /* SRC_PARAM_PARAM_H_ */
