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

	FILE * store = fopen(filename, "r");
	if (store == NULL)
		return;

	while (1) {

		char name[100];
		int id, node, type, size;

		int scanned = fscanf(store, "%25[^|]|%u:%u?%u[%d]\n", name, &id, &node, &type, &size);
		if (scanned < 0)
			break;

		//printf("scanned %u - %s|%u:%u?%u[%d]\n", scanned, name, id, node, type, size);

		param_t * param = param_list_find_id(node, id);
		if (param != NULL)
			continue;

		if (node == PARAM_LIST_LOCAL)
			continue;

		param = calloc(sizeof(param_t), 1);
		param->name = strdup(name);
		param->id = id;
		param->node = node;
		param->type = type;
		param->size = size;

		/* Remote parameter setup */
		param->storage_type = PARAM_STORAGE_REMOTE;
		param->value_get = calloc(param_size(param), 1);
		param->value_set = calloc(param_size(param), 1);

		//printf("Adding %s %u:%u\n", param->name, param->id, param->node);

		param_list_add(param);

	}

	fclose(store);

}

static int param_list_store_file_save_slash(struct slash *slash)
{
	param_list_store_file_save("param_store.bin");
	return SLASH_SUCCESS;
}
slash_command_sub(param, save, param_list_store_file_save_slash, NULL, NULL);
