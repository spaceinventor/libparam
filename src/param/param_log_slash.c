/*
 * param_log_slash.c
 *
 *  Created on: Mar 10, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>

#include "param_log.h"

static int param_log_scan_slash(struct slash *slash)
{
	param_log_scan();
	return SLASH_SUCCESS;
}
slash_command_sub(log, scan, param_log_scan_slash, NULL, NULL);
