/*
 * rparam_list_store_file.c
 *
 *  Created on: Nov 12, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <slash/slash.h>
#include <param/rparam.h>
#include <param/rparam_list.h>


void rparam_list_store_file_save(void) {

	FILE * store = fopen("rparam_store.bin", "w+");
	if (store == NULL)
		return;

	void add_rparam(rparam_t * rparam) {
		fwrite(rparam, 1, sizeof(rparam_t), store);
	}

	rparam_list_foreach(add_rparam);

	fclose(store);

}

void rparam_list_store_file_load(void) {

	FILE * store = fopen("rparam_store.bin", "r");
	if (store == NULL)
		return;

	rparam_t rparam = {};
	while (fread(&rparam, 1, sizeof(rparam_t), store) == sizeof(rparam_t)) {

		rparam_t * rparam_cpy = calloc(sizeof(rparam_t), 1);
		if (rparam_cpy == NULL)
			continue;
		memcpy(rparam_cpy, &rparam, sizeof(rparam_t));
		rparam_cpy->setvalue = NULL;
		rparam_cpy->setvalue_pending = 0;
		rparam_cpy->value = NULL;
		rparam_cpy->value_updated = 0;
		rparam_cpy->next = NULL;

		rparam_list_add(rparam_cpy);

	}

	fclose(store);

}

static int rparam_list_store_file_save_slash(struct slash *slash)
{
	rparam_list_store_file_save();
	return SLASH_SUCCESS;
}
slash_command_sub(rparam, savelist, rparam_list_store_file_save_slash, NULL, NULL);
