/*
 * param.h
 *
 *  Created on: Sep 21, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_H_
#define SRC_PARAM_PARAM_H_

#include <stdint.h>
#include <sys/queue.h>
#include <vmem/vmem.h>

/**
 * DATATYPES
 */

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

/**
 * Parameter description structure
 * Note: this is not packed in order to maximise run-time efficiency
 */
typedef struct param_s {

	/* Parameter declaration */
	uint16_t id;
	uint8_t node;
	param_type_e type;
	param_readonly_type_e readonly;
	char *name;
	char *unit;

	/* Storage */
	void * addr;
	const struct vmem_s * vmem;
	int array_size;
	int array_step;

	/* Local info */
	void (*callback)(struct param_s * param, int offset);
	uint32_t timestamp;

	SLIST_ENTRY(param_s) next;	// single linked list

} param_t;

#define PARAM_LIST_LOCAL	255

/**
 * DEFINITION HELPERS:
 *
 * These macros are one-liners that will simplify the layout of the parameter descriptions
 * found in source files.
 *
 * A parameter can reside either in VMEM or in RAM directly.
 *  For a RAM parameter the physaddr must be defined, and,
 *  for a VMEM parameter the vmem and addr's must be defined.
 *
 * The size field is only important for non-native types such as string, data and vector.
 *
 */
#define PARAM_DEFINE_STATIC_RAM(_id, _name, _type, _array_count, _array_step, _readonly, _callback, _unit, _physaddr, _log) \
	__attribute__((section("param."#_name))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.vmem = NULL, \
		.node = PARAM_LIST_LOCAL, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.array_size = _array_count, \
		.array_step = _array_step, \
		.readonly = _readonly, \
		.unit = _unit, \
		.callback = _callback, \
		.addr = _physaddr, \
	}

#define PARAM_DEFINE_STATIC_VMEM(_id, _name, _type, _array_count, _array_step, _readonly, _callback, _unit, _vmem_name, _vmem_addr, _log) \
	__attribute__((section("param."#_name))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.node = PARAM_LIST_LOCAL, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.array_size = _array_count, \
		.array_step = _array_step, \
		.readonly = _readonly, \
		.callback = _callback, \
		.unit = _unit, \
		.addr = (void *) _vmem_addr, \
		.vmem = &vmem_##_vmem_name, \
	}

#define PARAM_DEFINE_REMOTE(_name, _node, _id, _type, _array_size, _value_get) \
	char __attribute__((aligned(8))) _##_name##_value_get[_size]; \
	param_t _name = { \
		.storage_type = PARAM_STORAGE_REMOTE, \
		.node = _node, \
		.id = _id, \
		.type = _type, \
		.array_size = _array_size, \
		.name = (char *) #_name, \
		\
		.value_get = _##_name##_value_get, \
	};

/* Native getter functions, will return native types */
#define PARAM_GET(type, name) \
	type param_get_##name(param_t * param); \
	type param_get_##name##_array(param_t * param, unsigned int i);
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

/* Native setter functions, these take a native type as argument */
#define PARAM_SET(type, name) \
	void param_set_##name(param_t * param, type value); \
	void param_set_##name##_nocallback(param_t * param, type value); \
	void param_set_##name##_array(param_t * param, unsigned int i, type value); \
	void param_set_##name##_array_nocallback(param_t * param, unsigned int i, type value);
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

/* Non-native types needs to go through a function which includes a void pointer and the length */
void param_set_data(param_t * param, void * inbuf, int len);
void param_get_data(param_t * param, void * outbuf, int len);
#define param_set_string param_set_data
#define param_get_string param_get_data

/* Generic setter function:
 * This function can be used to set data of any type
 */
void param_set(param_t * param, unsigned int offset, void * value);
void param_get(param_t * param, unsigned int offset, void * value);

/* Returns the size of a native type */
int param_typesize(param_type_e type);
int param_size(param_t * param);

#endif /* SRC_PARAM_PARAM_H_ */
