/*
 * param.h
 *
 *  Created on: Sep 21, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_H_
#define SRC_PARAM_PARAM_H_

#include <stdint.h>
#include <vmem/vmem.h>

/**
 * DATATYPES
 */

typedef struct {
	float x;
	float y;
	float z;
} __attribute__((__packed__)) param_type_vector3;

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
	PARAM_TYPE_VECTOR3,
} param_type_e;

typedef enum {
	PARAM_READONLY_FALSE,         //! Not readonly
	PARAM_READONLY_TRUE,          //! Readonly Internal and external
	PARAM_READONLY_EXTERNAL,      //! Readonly External only
	PARAM_READONLY_INTERNAL,      //! Readonly Internal only
	PARAM_HIDDEN,                 //! Do not display on lists
} param_readonly_type_e;

typedef enum {
	PARAM_STORAGE_RAM,            //! Use local RAM access
	PARAM_STORAGE_VMEM,           //! Use VMEM read/write functions
	PARAM_STORAGE_REMOTE,         //! Use remote parameter service
} param_storage_type_e;

/**
 * Parameter description structure
 * Note: this is not packed in order to maximise run-time efficiency
 */
typedef struct param_s {

	/* Storage type:
	 * 0 = RAM, 1 = REMOTE, 2 = VMEM */
	param_storage_type_e storage_type;

	/* Parameter declaration */
	uint8_t node;
	uint16_t id;
	param_type_e type;
	int size;
	char *name;
	char *unit;

	/* Used for linked list */
	struct param_s * next;

	union {
		struct {
			union {
				struct {
				const struct vmem_s * vmem;
				int addr;
				};
				struct {
					void * physaddr;
				};
			};
			param_readonly_type_e readonly;
			void (*callback)(const struct param_s * param);
		};
		struct {
			uint16_t timeout;

			/* Used for rparam get/set */
			void * value_get;
			void * value_set;
			uint32_t value_updated; // Timestamp
			uint8_t value_pending; // 0 = none, 1 = pending, 2 = acked

		};
	};
} param_t;

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
#define PARAM_DEFINE_STATIC_RAM(_id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _physaddr) \
	__attribute__((section("param"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	const param_t _name = { \
		.storage_type = PARAM_STORAGE_RAM, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.readonly = _readonly, \
		.unit = _unit, \
		.callback = _callback, \
		.physaddr = _physaddr, \
	}

#define PARAM_DEFINE_STRUCT_RAM(fieldname, _id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _physaddr) \
	.fieldname = { \
		.storage_type = PARAM_STORAGE_RAM, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.readonly = _readonly, \
		.unit = _unit, \
		.callback = _callback, \
		.physaddr = _physaddr, \
	}


#define PARAM_DEFINE_STATIC_VMEM(_id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _vmem_name, _addr) \
	__attribute__((section("param"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	const param_t _name = { \
		.storage_type = PARAM_STORAGE_VMEM, \
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
		.storage_type = PARAM_STORAGE_VMEM, \
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

/* Native getter functions, will return native types */
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

/* Native setter functions, these take a native type as argument */
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

/* Non-native types needs to go through a function which includes a void pointer and the length */
void param_set_data(param_t * param, void * inbuf, int len);
void param_get_data(param_t * param, void * outbuf, int len);
#define param_set_string param_set_data
#define param_get_string param_get_data

/* Generic setter function:
 * This function can be used to set data of any type
 */
void param_set(param_t * param, void * value);

/* Print and list helpers */
void param_print(param_t * param);
void param_list(char * token);

/* Returns the size of a native type */
int param_typesize(param_type_e type);

#endif /* SRC_PARAM_PARAM_H_ */
