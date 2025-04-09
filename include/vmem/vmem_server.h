/*
 * vmem_server.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_

#include <sys/queue.h>

#include <csp/csp.h>
#include <vmem/vmem.h>

#define VMEM_SERVER_TIMEOUT 30000
#define VMEM_SERVER_MTU 192
#define VMEM_PORT_SERVER 14
#define VMEM_VERSION 3

typedef enum {
	VMEM_SERVER_UPLOAD,
	VMEM_SERVER_DOWNLOAD,
	VMEM_SERVER_LIST,
	VMEM_SERVER_RESTORE,
	VMEM_SERVER_BACKUP,
	VMEM_SERVER_UNLOCK,
	VMEM_SERVER_CALCULATE_CRC32,

	VMEM_SERVER_USER_TYPES = 0x80,
} vmem_request_type;

typedef struct {
	uint8_t version;
	uint8_t type;
} __attribute__((packed)) vmem_request_hdr_t;

typedef struct {
	union {
		struct {
			uint8_t version;
			uint8_t type;
		};
		vmem_request_hdr_t hdr;
	};
	union {
		uint8_t body[0];
		struct {
			uint32_t address;
			uint32_t length;
		} data;
		struct {
			uint64_t address;
			uint32_t length;
		} data2;
		struct {
			uint64_t address;
			uint64_t length;
		} data3;
		struct {
			uint8_t vmem_id;
		} vmem;
		struct {
			uint32_t code;
		} unlock;
	};
} __attribute__((packed)) vmem_request_t;

typedef struct {
	uint32_t vaddr;
	uint32_t size;
	uint8_t vmem_id;
	uint8_t type;
	char name[5];
} __attribute__((packed)) vmem_list_t;

typedef struct {
	uint64_t vaddr;
	uint32_t size;
	uint8_t vmem_id;
	uint8_t type;
	char name[5];
} __attribute__((packed)) vmem_list2_t;

typedef struct {
	uint64_t vaddr;
	uint64_t size;
	uint8_t vmem_id;
	uint8_t type;
	char name[16+1];
} __attribute__((packed)) vmem_list3_t;

typedef int vmem_handler_t(csp_conn_t *conn, csp_packet_t *packet, void *context);
typedef struct vmem_handler_obj_s {
	uint8_t type;
	void *context;
	vmem_handler_t *handler;
	SLIST_ENTRY( vmem_handler_obj_s ) list;
} vmem_handler_obj_t;

void vmem_server_handler(csp_conn_t * conn);
void vmem_server_loop(void * param);

/**
 * @brief Register a handler for a specific VMEM request type
 * 
 * Use this method for registering a handler to be called for a specific
 * type of VMEM request. The handler will be called with the request type,
 * the connection and packet as arguments, and the context pointer as the
 * fourth argument.
 * 
 * The caller must supply the memory for the handler object, which will be
 * inserted into a list of handlers. The handler object must be kept alive
 * until the handler is unregistered.
 * 
 * @param type The VMEM request type to bind the handler to
 * @param func A pointer to the handler function which gets called
 * @param obj Pointer to the handler object supplied by the caller
 * @param context A pointer to the context data to be passed to the handler
 * @return int 0=success, -1=error
 */
int vmem_server_bind_type(uint8_t type, vmem_handler_t *func, vmem_handler_obj_t *obj, void *context);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_ */
