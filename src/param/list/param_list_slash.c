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
#include <slash/optparse.h>
#include <slash/dflopt.h>

#include <libparam.h>
#include <param/param.h>
#include <param/param_list.h>
#include "param_list.h"
#include "../param_slash.h"

static int list(struct slash *slash)
{
    int node = slash_dfl_node;
    int verbosity = 1;
    char * maskstr = NULL;

    optparse_t * parser = optparse_new("list", "[name wildcard=*]");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_int(parser, 'v', "verbosity", "NUM", 0, &verbosity, "verbosity (default = 1, max = 3)");
    optparse_add_string(parser, 'm', "mask", "STR", &maskstr, "mask string");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    /* Interpret maskstring */
    uint32_t mask = 0xFFFFFFFF;
    if (maskstr != NULL) {
        mask = param_maskstr_to_mask(maskstr);
    }

    char * globstr = NULL;
    if (++argi < slash->argc) {
        globstr = slash->argv[argi];
    }

    param_list_print(mask, node, globstr, verbosity);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command(list, list, "[OPTIONS...] [name wildcard=*]", "List parameters");


static int list_download(struct slash *slash)
{
    unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 2;

    optparse_t * parser = optparse_new("list download", NULL);
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 2)");
    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (++argi < slash->argc) {
        printf("Node as argument, is deprecated\n");
        node = atoi(slash->argv[argi]);
    }

    param_list_download(node, timeout, version);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, download, list_download, "[node]", NULL);

static int list_forget(struct slash *slash)
{

    int node = slash_dfl_node;

    optparse_t * parser = optparse_new("list forget", NULL);
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (++argi < slash->argc) {
        printf("Node as argument, is deprecated\n");
        node = atoi(slash->argv[argi]);
    }

    printf("Removed %i parameters\n", param_list_remove(node, 1));

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(list, forget, list_forget, "[node]", "Forget remote parameters. Omit or set node to -1 to include all.");