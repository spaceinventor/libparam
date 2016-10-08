/*
 * param_slash.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <param/param.h>

static int param_slash_list(struct slash *slash)
{
	param_list(NULL);
	return SLASH_SUCCESS;
}
slash_command(param_list, param_slash_list, NULL, "List parameters");

