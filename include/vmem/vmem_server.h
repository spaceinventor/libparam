/*
 * vmem_server.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_

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
} vmem_request_type;

typedef struct {
	uint8_t version;
	uint8_t type;
	union {
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

void vmem_server_handler(csp_conn_t * conn);
void vmem_server_loop(void * param);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_ */
