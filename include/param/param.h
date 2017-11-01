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
	PARAM_STORAGE_TEMPLATE,       //! No storage (parameter template)
} param_storage_type_e;

typedef struct param_log_s {
	void * phys_addr;
	unsigned int phys_len;
	unsigned int in;
	unsigned int out;
	unsigned int elm_size;
	unsigned int elm_count;
} param_log_t;

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

	SLIST_ENTRY(param_s) next;	// single linked list

	param_log_t * log;

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
			void (*callback)(struct param_s * param);
		};
		struct {
			void * value_get;
			void * value_set;
			uint32_t value_remote_timestamp; // Remote timestamp
			uint32_t value_updated; // Timestamp (used by collector)
			uint8_t value_pending; // 0 = none, 1 = pending, 2 = acked
			unsigned int refresh; // Refresh interval in ms
		};
	};
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
#define PARAM_DEFINE_STATIC_RAM(_id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _physaddr, _log) \
	__attribute__((section("param."#_name))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.storage_type = PARAM_STORAGE_RAM, \
		.node = PARAM_LIST_LOCAL, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.readonly = _readonly, \
		.unit = _unit, \
		.callback = _callback, \
		.physaddr = _physaddr, \
		.log = _log, \
	}

#define PARAM_DEFINE_STRUCT_RAM(fieldname, _id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _physaddr, _log) \
	.fieldname = { \
		.storage_type = PARAM_STORAGE_RAM, \
		.node = PARAM_LIST_LOCAL, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.readonly = _readonly, \
		.unit = _unit, \
		.callback = _callback, \
		.physaddr = _physaddr, \
		.log = _log, \
	}


#define PARAM_DEFINE_STATIC_VMEM(_id, _name, _type, _size, _min, _max, _readonly, _callback, _unit, _vmem_name, _addr, _log) \
	__attribute__((section("param."#_name))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.storage_type = PARAM_STORAGE_VMEM, \
		.node = PARAM_LIST_LOCAL, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.size = _size, \
		.readonly = _readonly, \
		.callback = _callback, \
		.unit = _unit, \
		.addr = _addr, \
		.vmem = &vmem_##_vmem_name, \
		.log = _log, \
	}

#define PARAM_DEFINE_STRUCT_VMEM(fieldname, _id, _type, _size, _min, _max, _readonly, _callback, _unit, _vmem_name, _addr, _log) \
	.fieldname = { \
		.storage_type = PARAM_STORAGE_VMEM, \
		.node = PARAM_LIST_LOCAL, \
		.id = _id, \
		.type = _type, \
		.size = _size, \
		.name = #_vmem_name "_" #fieldname, \
		.readonly = _readonly, \
		.callback = _callback, \
		.unit = _unit, \
		.addr = _addr, \
		.vmem = &vmem_##_vmem_name, \
		.log = _log, \
	}

#define PARAM_DEFINE_REMOTE(_name, _node, _id, _type, _size, _value_get, _value_set) \
	param_t _name = { \
		.storage_type = PARAM_STORAGE_REMOTE, \
		.node = _node, \
		.id = _id, \
		.type = _type, \
		.size = _size, \
		.name = (char *) #_name, \
		\
		.value_get = _value_get, \
		.value_set = _value_set \
	};

#define PARAM_DEFINE_STATIC_REMOTE_READONLY(_name, _node, _id, _type, _size) \
	char __attribute__((aligned(8))) _##_name##_value_get[_size]; \
	PARAM_DEFINE_REMOTE(_name, _node, _id, _type, _size, _##_name##_value_get, NULL)

#define PARAM_DEFINE_STATIC_REMOTE_READWRITE(_name, _node, _id, _type, _size) \
	char __attribute__((aligned(8))) _##_name##_value_get[_size]; \
	char __attribute__((aligned(8))) _##_name##_value_set[_size]; \
	PARAM_DEFINE_REMOTE(_name, _node, _id, _type, _size, _##_name##_value_get, _##_name##_value_set)

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

/* Print and list helpers */
void param_print_header(int nodes[], int nodes_count);
void param_print(param_t * param, int offset, int nodes[], int nodes_count, int verbose);
void param_list(char * token);

/* Returns the size of a native type */
int param_typesize(param_type_e type);
int param_size(param_t * param);

#endif /* SRC_PARAM_PARAM_H_ */
