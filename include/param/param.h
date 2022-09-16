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

#include "libparam.h"

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
	PARAM_TYPE_INVALID,
} param_type_e;

/**
 * Global parameter mask
 */

#define PM_READONLY             (1 << 0) //! r: Readonly by any
#define PM_REMOTE               (1 << 1) //! R: Remote parameter
#define PM_CONF                 (1 << 2) //! c: Actual settings, to be modified by a human (excluding network config)
#define PM_TELEM                (1 << 3) //! t: Ready-to-use telemetry, converted to human readable.
#define PM_HWREG                (1 << 4) //! h: Raw-bit-values in external chips
#define PM_ERRCNT               (1 << 5) //! e: Rarely updated error counters (hopefully)
#define PM_SYSINFO              (1 << 6) //! i: Boot information, time
#define PM_SYSCONF              (1 << 7) //! C: Network and time configuration
#define PM_WDT                  (1 << 8) //! w: Crictical watchdog
#define PM_DEBUG                (1 << 9) //! d: Debug flag (enables uart output)
#define PM_CALIB               (1 << 10) //! q: Calibration gains and offsets

/* Atomic write:
 * If this flag is set, the receiver must enter a critical region before writing to
 * the parameter memory. The critical region will last to the end of the push packet
 * So putting the atomic parameters at the end of the push request will reduce the time
 * spent in critical region.
 */
#define PM_ATOMIC_WRITE        (1 << 11) //! o: Parameter must be written atomically.
#define PM_PRIO1               (1 << 12) //! q: Priority of parameter for telemetry logging (two bits)
#define PM_PRIO2               (2 << 12) //! q: Priority of parameter for logging and retrieval (two bits)
#define PM_PRIO3               (3 << 12) //! q: Priority of parameter for logging and retrieval (two bits)
#define PM_PRIO_MASK           (3 << 12) //! q: Priority of parameter for logging and retrieval (two bits)

/* Reserved flags:
 * Lower 16 is parameter system, upper 16 are user flags  */
#define PM_PARAM_FLAGS        0x0000FFFF //! Lower 16-bits are reserved for parameter system and major class flags
#define PM_USER_FLAGS         0xFFFF0000 //! Upper 16-bits are reserved for user

/**
 * Parameter description structure
 * Note: this is not packed in order to maximise run-time efficiency
 */
typedef struct param_s {

	/* Parameter declaration */
	uint16_t id;
	uint16_t node;
	param_type_e type;
	uint32_t mask;
	char *name;
	char *unit;
	char *docstr;

	/* Storage */
	void * addr;
	struct vmem_s * vmem;
	int array_size;
	int array_step;

	/* Local info */
	void (*callback)(struct param_s * param, int offset);
	uint32_t timestamp;

#ifdef PARAM_HAVE_SYS_QUEUE
	/* single linked list:
	 * The weird definition format comes from sys/queue.h SLINST_ENTRY() macro */
	struct { struct param_s *sle_next; } next;
#endif


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
#define PARAM_DEFINE_STATIC_RAM(_id, _name, _type, _array_count, _array_step, _flags, _callback, _unit, _physaddr, _docstr) \
	__attribute__((section("param"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.vmem = NULL, \
		.node = 0, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.array_size = _array_count, \
		.array_step = _array_step, \
		.mask = _flags, \
		.unit = _unit, \
		.callback = _callback, \
		.addr = _physaddr, \
		.docstr = _docstr, \
	}

#define PARAM_DEFINE_STATIC_VMEM(_id, _name, _type, _array_count, _array_step, _flags, _callback, _unit, _vmem_name, _vmem_addr, _docstr) \
	__attribute__((section("param"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.node = 0, \
		.id = _id, \
		.type = _type, \
		.name = #_name, \
		.array_size = _array_count, \
		.array_step = _array_step, \
		.mask = _flags, \
		.callback = _callback, \
		.unit = _unit, \
		.addr = (void *) _vmem_addr, \
		.vmem = &vmem_##_vmem_name, \
		.docstr = _docstr, \
	}

#define PARAM_REMOTE_NODE_IGNORE 16382

#define PARAM_DEFINE_REMOTE(_name, _node, _id, _type, _array_size, _array_step, _flags, _physaddr, _docstr) \
	__attribute__((section("param"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_t _name = { \
		.node = _node, \
		.id = _id, \
		.type = _type, \
		.array_size = _array_size, \
		.array_step = _array_step, \
		.name = (char *) #_name, \
		.mask = _flags, \
		.addr = _physaddr, \
		.docstr = _docstr, \
	};

#define PARAM_DEFINE_REMOTE_DYNAMIC(_id, _name, _node, _type, _array_size, _array_step, _flags, _physaddr, _docstr) \
	param_t _name = { \
		.node = _node, \
		.id = _id, \
		.type = _type, \
		.array_size = _array_size, \
		.array_step = _array_step, \
		.name = (char *) #_name, \
		.mask = _flags, \
		.addr = _physaddr, \
		.docstr = _docstr, \
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
void param_set_data_nocallback(param_t * param, void * inbuf, int len);
void param_get_data(param_t * param, void * outbuf, int len);
void param_set_string(param_t * param, void * inbuf, int len);
#define param_get_string param_get_data

/* Generic setter function:
 * This function can be used to set data of any type
 */
void param_set(param_t * param, unsigned int offset, void * value);
void param_get(param_t * param, unsigned int offset, void * value);

/* Returns the size of a native type */
int param_typesize(param_type_e type);
int param_size(param_t * param);

/* Copies from one parameter to another */
void param_copy(param_t * dest, param_t * src);

/* External hooks to get atomic writes */
extern __attribute__((weak)) void param_enter_critical(void);
extern __attribute__((weak)) void param_exit_critical(void);

#endif /* SRC_PARAM_PARAM_H_ */
