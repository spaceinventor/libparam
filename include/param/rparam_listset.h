/*
 * rparam_listset.h
 *
 *  Created on: Nov 21, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_RPARAM_LISTSET_H_
#define LIB_PARAM_INCLUDE_PARAM_RPARAM_LISTSET_H_

typedef struct rparam_list_s {
	char * listname;
	struct rparam_list_s * next;
	char * names[];
} rparam_listset_t;

int rparam_listset_add(rparam_listset_t * item);
rparam_listset_t * rparam_listset_find(char * name);

#endif /* LIB_PARAM_INCLUDE_PARAM_RPARAM_LISTSET_H_ */
