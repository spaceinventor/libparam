/*
 * rparam_list.h
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_LIST_H_
#define LIB_PARAM_SRC_PARAM_PARAM_LIST_H_

#include <param/param.h>

#define PARAM_LIST_LOCAL	255

int param_list_add(param_t * item);
param_t * param_list_find_id(int node, int id);
param_t * param_list_find_name(int node, char * name);
void param_list_download(int node, int timeout);
void param_list_print(char * token);
void param_list_foreach(int (*iterator)(param_t * rparam));

param_t * param_create_remote(int id, int node, int type, int size, char * name, int namelen);
void param_free(param_t * param);

#endif /* LIB_PARAM_SRC_PARAM_PARAM_LIST_H_ */
