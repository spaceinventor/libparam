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
#include <slash/slash.h>

#include "param_list.h"

// TODO: Add node filter
// TODO: Add remote only

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

	param_list_from_string(stream, -1);

	fclose(stream);

}

// TODO: Add filename, node_filter and remote_only as input
static int param_list_store_file_save_slash(struct slash *slash)
{
	param_list_store_file_save("param_store.bin");
	return SLASH_SUCCESS;
}
slash_command_subsub(param, list, save, param_list_store_file_save_slash, NULL, NULL);

// TODO: Add file load
