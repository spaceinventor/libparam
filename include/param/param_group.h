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

	param_t const* *params;				// List of params (could exist in flash or ram)
	uint8_t storage_dynamic;			// 0 = static, 1 = dynamic
	uint32_t storage_max_count;			// Maximum number of parameters in list (used for dynamic only)

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
		.vmem = NULL, \
		.params = _params, \
		.count = sizeof(_params) / sizeof(_params[0]), \
	};

param_group_t * param_group_iterate(param_group_iterator * iterator);
param_group_t * param_group_create(char * name, int max_count);
param_group_t * param_group_find_name(char * name);
void param_group_param_add(param_group_t *group, param_t *param);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_GROUP_H_ */
