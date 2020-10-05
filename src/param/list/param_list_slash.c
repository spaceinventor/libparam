/*
 * param_list_slash.c
 *
 *  Created on: Dec 14, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <libparam.h>
#include <param/param.h>
#include <param/param_list.h>
#include "param_list.h"
#include "../param_slash.h"

static int list(struct slash *slash)
{
    uint32_t mask = 0xFFFFFFFF;
    if (slash->argc > 1)
        mask = param_maskstr_to_mask(slash->argv[1]);
    param_list_print(mask);
    return SLASH_SUCCESS;
}
slash_command(list, list, "[str]", "List parameters");

static int list_download(struct slash *slash)
{
    if (slash->argc < 2)
        return SLASH_EUSAGE;

    unsigned int node = atoi(slash->argv[1]);
    unsigned int timeout = 1000;
    unsigned int version = 2;
    if (slash->argc >= 3)
        timeout = atoi(slash->argv[2]);
    if (slash->argc >= 4)
        version = atoi(slash->argv[3]);

    param_list_download(node, timeout, version);

    return SLASH_SUCCESS;
}
slash_command_sub(list, download, list_download, "<node> [timeout] [version]", NULL);
