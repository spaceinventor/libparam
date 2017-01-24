/*
 * param_group_store_file.c
 *
 *  Created on: Nov 12, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <slash/slash.h>

#include "param_group.h"

void param_group_store_file_save(char * filename) {

	FILE * store = fopen(filename, "w+");
	if (store == NULL)
		return;
	param_group_to_string(store, NULL);
	fclose(store);

}

void param_group_store_file_load(char * filename) {

	FILE * stream = fopen(filename, "r");
	if (stream == NULL)
		return;
	param_group_from_string(stream);
	fclose(stream);

}
