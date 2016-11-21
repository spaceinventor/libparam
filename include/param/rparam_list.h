/*
 * rparam_list.h
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_RPARAM_LIST_H_
#define LIB_PARAM_SRC_PARAM_RPARAM_LIST_H_

typedef struct rparam_list_s {
	char * listname;
	struct rparam_list_s * next;
	char * names[];
} rparam_list_t;

int rparam_list_add(rparam_list_t * item);
rparam_list_t * rparam_list_find(char * name);

int rparam_add(rparam_t * item);
rparam_t * rparam_list_find_id(int node, int id);
rparam_t * rparam_list_find_name(int node, char * name);
void rparam_list_download(int node, int timeout);
void rparam_list_print(int node_filter, int pending);
void rparam_list_foreach(void (*iterator)(rparam_t * rparam));

#endif /* LIB_PARAM_SRC_PARAM_RPARAM_LIST_H_ */
