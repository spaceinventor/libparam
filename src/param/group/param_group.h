/*
 * param_group.h
 *
 *  Created on: Jan 13, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_GROUP_PARAM_GROUP_H_
#define LIB_PARAM_SRC_PARAM_GROUP_PARAM_GROUP_H_

#include <vmem/vmem.h>

void param_group_from_string(FILE *stream);
void param_group_to_string(FILE * stream, char * group_name);

void param_group_store_vmem_save(vmem_t * vmem);
void param_group_store_vmem_load(vmem_t * vmem);

#endif /* LIB_PARAM_SRC_PARAM_GROUP_PARAM_GROUP_H_ */
