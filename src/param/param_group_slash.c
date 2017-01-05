/*
 * param_group_slash.c
 *
 *  Created on: Jan 5, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <slash/slash.h>

#include <param/param_group.h>

static int groups(struct slash *slash)
{
	param_group_print();
	return SLASH_SUCCESS;
}
slash_command(groups, groups, NULL, NULL);
