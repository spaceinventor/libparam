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

typedef struct param_list_iterator_s {
	int phase;							// Hybrid iterator has multiple phases (0 == Static, 1 == Dynamic List)
	param_t * element;
} param_list_iterator;

param_t * param_list_iterate(param_list_iterator * iterator);

int param_list_add(param_t * item);

/**
 * @brief Remove remote parameters, matching the provided arguments, from the global list.
 *
 * @param node Remove parameters from this node. Use <1 for all nodes.
 * @param name Optional wildcard name pattern to filter parameters by.
 * @return Count of parameters affected.
 */
int param_list_remove(int node, uint8_t verbose);
param_t * param_list_find_id(int node, int id);
param_t * param_list_find_name(int node, char * name);
void param_list_print(uint32_t mask, int node, char * globstr, int verbosity);
uint32_t param_maskstr_to_mask(char * str);

param_t * param_list_from_line(char * line);

/**
 * @brief 
 * 
 * @param id 					parameter is 0-1023
 * @param node 					csp node, 0 = local, 1+ = remote
 * @param type 					PARAM_TYPE_.... enum
 * @param mask 					PM_CONF, PM_TELEM, and friends
 * @param array_size 			Number of elements in array
 * @param name 					name, max 36 long
 * @param unit 					unit, max 10 long
 * @param help 					Help string, max 150 long
 * @param storage_type 			Set to -1 for unspecified, 0 = RAM, 1-... see vmem types
 * @return param_t*             Pointer to created object in memory. Must be freed again
 */
param_t * param_list_create_remote(int id, int node, int type, uint32_t mask, int array_size, char * name, char * unit, char * help, int storage_type);

void param_list_destroy(param_t * param);
void param_print(param_t * param, int offset, int nodes[], int nodes_count, int verbose);

unsigned int param_list_packed_size(int list_version);
int param_list_unpack(int node, void * data, int length, int list_version);
int param_list_pack(void* buf, int buf_size, int prio_only, int remote_only, int list_version);

/**
 * @return -1 for connection errors, otherwise returns the number of parameters downloaded.
 */
int param_list_download(int node, int timeout, int list_version);

/* From param_list_store_file.c */
void param_list_store_file_save(char * filename);
void param_list_store_file_load(char * filename);

/* From param_list_store_vmem.c */
void param_list_store_vmem_save(vmem_t * vmem);
void param_list_store_vmem_load(vmem_t * vmem);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_LIST_H_ */
