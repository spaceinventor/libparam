/*
 * rparam_list_store_file.c
 *
 *  Created on: Nov 12, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <slash/slash.h>


void rparam_list_store_file_save(void) {

	FILE * store = fopen("rparam_store.bin", "w+");
	if (store == NULL)
		return;

	int add_rparam(param_t * param) {
		fwrite(param, 1, sizeof(param_t), store);
		return 1;
	}

	param_list_foreach(add_rparam);

	fclose(store);

}

void rparam_list_store_file_load(void) {

	FILE * store = fopen("rparam_store.bin", "r");
	if (store == NULL)
		return;

	param_t param = {};
	while (fread(&param, 1, sizeof(param_t), store) == sizeof(param_t)) {

		param_t * param_cpy = calloc(sizeof(param_t), 1);
		if (param_cpy == NULL)
			continue;
		memcpy(param_cpy, &param, sizeof(param_t));
		param_cpy->value_set = NULL;
		param_cpy->value_pending = 0;
		param_cpy->value_get = NULL;
		param_cpy->value_updated = 0;
		param_cpy->next = NULL;

		param_list_add(param_cpy);

	}

	fclose(store);

}

static int rparam_list_store_file_save_slash(struct slash *slash)
{
	rparam_list_store_file_save();
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, savelist, rparam_list_store_file_save_slash, NULL, NULL);
