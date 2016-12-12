/*
 * rparam_list.h
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_RPARAM_LIST_H_
#define LIB_PARAM_SRC_PARAM_RPARAM_LIST_H_

#include <param/param.h>
#include <param/rparam.h>

int rparam_list_add(param_t * item);
param_t * rparam_list_find_id(int node, int id);
param_t * rparam_list_find_name(int node, char * name);
void rparam_list_download(int node, int timeout);
void rparam_list_print(char * token);
void rparam_list_foreach(void (*iterator)(param_t * rparam));

#endif /* LIB_PARAM_SRC_PARAM_RPARAM_LIST_H_ */
