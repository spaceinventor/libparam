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
#include <csp/arch/csp_thread.h>

typedef struct param_group_s {

	char name[10];
	uint8_t count;						// Number of params in group

	param_t **params;					// List of params (could exist in flash or ram)
	uint8_t storage_dynamic;			// 0 = static, 1 = dynamic
	uint32_t storage_max_count;			// Maximum number of parameters in list (used for dynamic only)

	uint32_t interval;					// Used for beacon
	uint8_t node;						// Used for beacon
	uint8_t port;						// Used for beacon
	uint32_t last_beacon;				// Used for beacon

	SLIST_ENTRY(param_group_s) next;	// single linked list

} param_group_t;

typedef struct param_group_iterator_s {
	int phase;							// Hybrid iterator has multiple phases (0 == Static, 1 == Dynamic List)
	param_group_t * element;
} param_group_iterator;

#define PARAM_GROUP_STATIC_PARAMS(_name, _params) \
	__attribute__((section("groups"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	param_group_t _name##_group = { \
		.name = #_name, \
		.params = (param_t **) _params, \
		.count = sizeof(_params) / sizeof(_params[0]), \
	};

param_group_t * param_group_iterate(param_group_iterator * iterator);
param_group_t * param_group_create(char * name, int max_count);
param_group_t * param_group_find_name(char * name);
void param_group_param_add(param_group_t *group, param_t *param);

csp_thread_return_t param_group_beacon_task(void *pvParameters);
int param_group_push(param_group_t * group, int host, int timeout);
int param_group_pull(param_group_t * group, int host, int timeout);
int param_group_copy(param_group_t * group, int host);

void param_group_store_vmem_save(vmem_t * vmem);
void param_group_store_vmem_load(vmem_t * vmem);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_GROUP_H_ */
