/*
 * param_log.c
 *
 *  Created on: Jan 4, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <param/param.h>
#include "param_string.h"
#include "param_log.h"

#include <csp/csp.h>

void param_log(param_t * param, void * new_value) {

	if (param->log == NULL)
		return;

	char new_value_str[41] = {};
	param_var_str(param->type, param->size, new_value, new_value_str, 40);

	char old_value_str[41] = {};
	param_value_str(param, old_value_str, 40);
	printf("param_log: %s %s => %s (%p[%u])\n", param->name, old_value_str, new_value_str, param->log->phys_addr, param->log->phys_len);

	/* Compare values */
	char old_value[param_size(param)];
	param_get_data(param, old_value, param_size(param));

	//csp_hex_dump("old", old_value, param_size(param));
	//csp_hex_dump("new", new_value, param_size(param));

	if (memcmp(old_value, new_value, param_size(param)) == 0) {
		printf("No change\n");
		return;
	}

	printf("Change detected, logging now\n");

}


