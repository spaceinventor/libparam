/*
 * rparam_listset.c
 *
 *  Created on: Nov 21, 2016
 *      Author: johan
 */

#include <string.h>

#include <param/rparam_listset.h>

static rparam_listset_t * listset_begin = NULL;
static rparam_listset_t * listset_end = NULL;

int rparam_listset_add(rparam_listset_t * item) {

	if (listset_begin == NULL)
		listset_begin = item;

	if (listset_end != NULL)
		listset_end->next = item;

	listset_end = item;

	item->next = NULL;

	return 0;

}

rparam_listset_t * rparam_listset_find(char * name) {

	rparam_listset_t * list = listset_begin;
	while(list != NULL) {

		if (strcmp(list->listname, name) == 0)
			return list;

		list = list->next;
	}
	return NULL;
}


