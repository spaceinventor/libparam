/*
 * rparam_list_store_file.c
 *
 *  Created on: Nov 12, 2016
 *      Author: johan
 */

#include <param/rparam.h>
#include <param/rparam_list.h>

void rparam_list_store_file_save(void) {

	void add_rparam(rparam_t * rparam) {

		printf("Adding rparam %s to file\n", rparam->name);
	}

	rparam_list_foreach(add_rparam);

}

void rparam_list_store_file_load(void) {

}
