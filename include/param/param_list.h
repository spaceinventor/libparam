/*
 * rparam_list.h
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_PARAM_LIST_H_
#define LIB_PARAM_INCLUDE_PARAM_PARAM_LIST_H_

#include <param/param.h>
#include <vmem/vmem.h>

#define PARAM_LIST_LOCAL	255

typedef struct param_list_iterator_s {
	int phase;							// Hybrid iterator has multiple phases (0 == Static, 1 == Dynamic List)
	param_t * element;
} param_list_iterator;

param_t * param_list_iterate(param_list_iterator * iterator);

int param_list_add(param_t * item);
param_t * param_list_find_id(int node, int id);
param_t * param_list_find_name(int node, char * name);
void param_list_print(uint32_t mask);
uint32_t param_maskstr_to_mask(char * str);

param_t * param_list_from_line(char * line);
param_t * param_list_create_remote(int id, int node, int type, uint32_t mask, int array_size, char * name, int namelen);
void param_list_destroy(param_t * param);

void param_list_download(int node, int timeout);


/* From param_list_store_file.c */
void param_list_store_file_save(char * filename);
void param_list_store_file_load(char * filename);

/* From param_list_store_vmem.c */
void param_list_store_vmem_save(vmem_t * vmem);
void param_list_store_vmem_load(vmem_t * vmem);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_LIST_H_ */
