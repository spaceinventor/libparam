/*
 * param_list_store_file.c
 *
 *  Created on: Nov 12, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <param/param_list.h>
#include <param/param_server.h>

#include "param_list.h"

// TODO: Add node filter
// TODO: Add remote only

static void param_list_from_string(FILE *stream) {

	char line[100] = {};
	while(fgets(line, 100, stream) != NULL) {
		param_list_from_line(line);
	}
}

static void param_list_to_string(FILE * stream, int node_filter, int remote_only) {

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if ((node_filter >= 0) && (param->node != node_filter))
			continue;

		if ((remote_only) && (param->node != PARAM_LIST_LOCAL))
			continue;

		int node = param->node;
		if (node == PARAM_LIST_LOCAL)
			node = csp_get_address();

		fprintf(stream, "%s|%u:%u?%u[%d]\n", param->name, param->id, node, param->type, param->array_size);
	}

}

void param_list_store_file_save(char * filename) {

	FILE * store = fopen(filename, "w+");
	if (store == NULL)
		return;

	param_list_to_string(store, -1, 1);

	fclose(store);

}

void param_list_store_file_load(char * filename) {

	FILE * stream = fopen(filename, "r");
	if (stream == NULL)
		return;

	param_list_from_string(stream);

	fclose(stream);

}
