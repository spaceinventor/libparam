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


void param_list_store_file_save(char * filename) {

	FILE * store = fopen(filename, "w+");
	if (store == NULL)
		return;

	int add_rparam(param_t * param) {
		fprintf(store, "%s|%u:%u?%u[%d]\n", param->name, param->id, param->node, param->type, param->size);
		return 1;
	}

	param_list_foreach(add_rparam);

	fclose(store);

}

void param_list_store_file_load(char * filename) {

	FILE * stream = fopen(filename, "r");
	if (stream == NULL)
		return;

	param_list_from_string(stream, -1);

	fclose(stream);

}

static int param_list_store_file_save_slash(struct slash *slash)
{
	param_list_store_file_save("param_store.bin");
	return SLASH_SUCCESS;
}
slash_command_sub(param, save, param_list_store_file_save_slash, NULL, NULL);
