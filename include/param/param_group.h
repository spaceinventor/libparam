/*
 * param_group.h
 *
 *  Created on: Jan 5, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_PARAM_GROUP_H_
#define LIB_PARAM_INCLUDE_PARAM_PARAM_GROUP_H_

#include <sys/queue.h>
#include <vmem/vmem.h>

typedef struct param_group_s {
	char name[10];
	uint32_t interval;					// Used for periodic pull
	uint8_t node;						// Used for periodic pull
	uint8_t port;						// Used for periodic pull
	uint8_t count;						// Number of params in group
	vmem_t *vmem;						// Set vmem to NULL for direct RAM access
	uint16_t const *ids;				// Pointer in RAM/VMEM
	param_t const* *params;				// List of params (only used for static storage)
	SLIST_ENTRY(param_group_s) next;	// single linked list
} param_group_t;

#define PARAM_GROUP_STATIC_IDS(_name, _ids) \
	__attribute__((section("groups"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_group_t _name##_group = { \
		.name = #_name, \
		.vmem = NULL, \
		.ids = _ids, \
		.count = sizeof(_ids) / sizeof(_ids[0]), \
	};

#define PARAM_GROUP_STATIC_PARAMS(_name, _params) \
	__attribute__((section("groups"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_group_t _name##_group = { \
		.name = #_name, \
		.vmem = NULL, \
		.params = _params, \
		.count = sizeof(_params) / sizeof(_params[0]), \
	};


void param_group_init(void);
void param_group_foreach(int (*iterator)(param_group_t * param));
void param_group_print(void);
int param_group_add(param_group_t * item);
param_group_t * param_group_find_name(char * name);
param_group_t * param_group_create(char * name);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_GROUP_H_ */
