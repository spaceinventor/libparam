/*
 * param_slash.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include <slash/completer.h>

#include <csp/csp.h>
#include <csp/csp_hooks.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <param/param_string.h>

#include "param_slash.h"
#include "param_wildcard.h"

static char queue_buf[PARAM_SERVER_MTU];
param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_EMPTY, .version = 2 };

static void param_slash_parse(char * arg, int node, param_t **param, int *offset) {

	/* Search for the '@' symbol:
	 * Call strtok twice in order to skip the stuff head of '@' */
	char * saveptr;
	char * token;

	/* Search for the '[' symbol: */
	strtok_r(arg, "[", &saveptr);
	token = strtok_r(NULL, "[", &saveptr);
	if (token != NULL) {
		sscanf(token, "%d", offset);
		*token = '\0';
	}

	char *endptr;
	int id = strtoul(arg, &endptr, 10);

	if (*endptr == '\0') {
		*param = param_list_find_id(node, id);
	} else {
		*param = param_list_find_name(node, arg);
	}

	return;

}

static void param_completer(struct slash *slash, char * token) {

	int node = slash_dfl_node;

	int matches = 0;
	size_t prefixlen = -1;
	param_t *prefix = NULL;
	char * orig_slash_buf = NULL;
	char *skip_prefix = NULL;

	/* TODO: find better way than hardcoding the command names */
	if (!strncmp(slash->buffer, "get", strlen("get"))) {
		skip_prefix = "get";
	} else if (!strncmp(slash->buffer, "set", strlen("set"))) {
		skip_prefix = "set";
	} else if (!strncmp(slash->buffer, "cmd add", strlen("cmd add"))) {
		skip_prefix = "cmd add";
	}

	if (skip_prefix == NULL) {
		fprintf(stderr, "%s() does not support the line/command: '%s'", __func__, slash->buffer);
		assert(false);  // Assert because this should be a compile-time error on part of the programmer, not the user.
		return;
	}

	{   /* Keep optparse parser in local scope, to prevent use-after-free */

		#define SLASH_ARG_MAX		16	/* Maximum number of arguments */
		char *argv[SLASH_ARG_MAX];
		int argc = 0;
		char *args = strdup(slash->buffer + strlen(skip_prefix));
		if (args == NULL) {
			fprintf(stderr, "No memory for tab-completion\n");
			return;
		}

		/* Build args */
		int slash_build_args(char *args, char **argv, int *argc);
		if (slash_build_args(args, argv, &argc) < 0) {
			argv[argc] = NULL;
			slash_printf(slash, "Mismatched quotes\n");
			//return -EINVAL;
		}

		optparse_t * parser = optparse_new(skip_prefix, "<name>[offset] <value>");
		optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");

		/* TODO: Slash doesn't prepare arguments for completer functions.
			A quick attempt to make it do this, seems to cause other problems for tab-completion.
			So we just do it ourselves here, for now. */
		//int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
		int argi = optparse_parse(parser, argc - 1, (const char **) argv + 1);
		optparse_del(parser);
		if (argi < 0) {
			return;
		}
	}

	if (skip_prefix) {
		orig_slash_buf = slash->buffer;
		slash_completer_skip_flagged_prefix(slash, skip_prefix);
		token = slash->buffer;
	}

	size_t tokenlen = strlen(token);

	const bool token_has_wildcard = has_wildcard(token, strlen(token));  // This will be true for every iteration, so we store it here.
	unsigned int num_params_on_node = 0;  // Used to inform the user when they've forgotten to 'list download' :)

	param_t * param;
	param_list_iterator i = { };
	while ((param = param_list_iterate(&i)) != NULL) {

		/* Don't print or complete parameters from other nodes.
			If the user wants to see all parameters, they can do 'list -n -1' */
		if (param-> node != node)
			continue;

		num_params_on_node++;

		/* Don't complete parameters when globbing is involved */
		if (token_has_wildcard) {

			/* Only print parameters where name matches (with or w/o wildcards) */
			if (strmatch(param->name, token, strlen(param->name), strlen(token))) {
				param_print(param, -1, NULL, 0, 2, 0);
			}

			/* We don't actually tab-complete with globbing,
			because it's ambiguous with multiple matches. */
			continue;
		}

		if (tokenlen > strlen(param->name))
			continue;

		if (strncmp(token, param->name,
				slash_min(strlen(param->name), tokenlen)) == 0) {

			/* Count matches */
			matches++;

			/* Find common prefix */
			if (prefixlen == (size_t) -1) {
				prefix = param;
				prefixlen = strlen(prefix->name);
			} else {
				size_t new_prefixlen = slash_prefix_length(prefix->name,
						param->name);
				if (new_prefixlen < prefixlen)
					prefixlen = new_prefixlen;
			}

			/* Print newline on first match */
			if (matches == 1)
				slash_printf(slash, "\n");

			/* Print param */
			param_print(param, -1, NULL, 0, 2, 0);

		}
	}

	if (num_params_on_node == 0) {
		static unsigned int tab_count = 1;
		static time_t tab_expiration = 0;

		if (tab_expiration == 0) {
			tab_expiration = time(NULL) + 60;  // User has to press 23 times, within 60 seconds.
		} else if (time(NULL) > tab_expiration) {
			tab_expiration = 0;
			tab_count = 1;
		}

		if (tab_count++ % 23 == 0) {
			/* No I'm not trying to introduce easter eggs into CSH, I don't know what you're talking about... */
			printf("♫ Ain't nobody here but us chickens ♫ Maybe do a little 'list download [node]'?\n");
		} else {
			printf("No known parameters on node %d, perhaps you've forgotten to 'list download [node]'?\n", node);
		}
	}

	if (!matches) {
		slash_bell(slash);
	} else {
		strncpy(token, prefix->name, prefixlen);
		token[prefixlen] = 0;
		slash->cursor = slash->length = (token - slash->buffer) + prefixlen;
	}

	if (skip_prefix) slash_completer_revert_skip(slash, orig_slash_buf);
}

static int cmd_get(struct slash *slash) {

	int node = slash_dfl_node;
	int paramver = 2;
	int server = 0;

    optparse_t * parser = optparse_new("get", "<name>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_int(parser, 's', "server", "NUM", 0, &server, "server to get parameters from (default = node))");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Check if name is present */
	if (++argi >= slash->argc) {
        optparse_del(parser);
		printf("missing parameter name\n");
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];
	int offset = -1;
	param_t * param = NULL;

	if (++argi != slash->argc) {
		optparse_del(parser);
		printf("too many arguments to command\n");
		return SLASH_EINVAL;
	}

	/* Go through the list of parameters */
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		/* Name match (with wildcard) */
		if (strmatch(param->name, name, strlen(param->name), strlen(name)) == 0) {
			continue;
		}

		/* Node match */
		if (param->node != node) {
			continue;
		}

		/* Local parameters are printed directly */
		if ((param->node == 0) && (server == 0)) {
			param_print(param, -1, NULL, 0, 0, 0);
			continue;
		}

		/* Select destination, host overrides parameter node */
		int dest = node;
		if (server > 0)
			dest = server;

		if (param_pull_single(param, offset, CSP_PRIO_HIGH, 1, dest, slash_dfl_timeout, paramver) < 0) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
		
	}

    optparse_del(parser);
	return SLASH_SUCCESS;

}
slash_command_completer(get, cmd_get, param_completer, "<param>", "Get");

static int cmd_set(struct slash *slash) {
	
	int node = slash_dfl_node;
	int paramver = 2;
	int server = 0;
	int ack_with_pull = true;
	int force = false;

    optparse_t * parser = optparse_new("set", "<name>[offset] <value>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_int(parser, 's', "server", "NUM", 0, &server, "server to set parameters on (default = node)");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");
    optparse_add_set(parser, 'f', "force", true, &force, "force setting readonly params");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char * name = slash->argv[argi];
	int offset = -1;
	param_t * param = NULL;
	param_slash_parse(name, node, &param, &offset);

	if (param == NULL) {
		printf("%s not found\n", name);
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (param->mask & PM_READONLY && !force) {
		printf("--force is required to set a readonly parameter\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Check if Value is present */
	if (++argi >= slash->argc) {
		printf("missing parameter value\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	char valuebuf[128] __attribute__((aligned(16))) = { };
	if (param_str_to_value(param->type, slash->argv[argi], valuebuf) < 0) {
		printf("invalid parameter value\n");
	    optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Local parameters are set directly */
	if (param->node == 0) {

		if (offset < 0 && param->type != PARAM_TYPE_STRING && param->type != PARAM_TYPE_DATA) {
			for (int i = 0; i < param->array_size; i++)
				param_set(param, i, valuebuf);
		} else {
			param_set(param, offset, valuebuf);
		}
		param_print(param, -1, NULL, 0, 2, 0);
	} else {

		/* Select destination, host overrides parameter node */
		int dest = node;
		if (server > 0)
			dest = server;
		csp_timestamp_t time_now;
		csp_clock_get_time(&time_now);
		*param->timestamp = 0;
		if (param_push_single(param, offset, valuebuf, 0, dest, slash_dfl_timeout, paramver, ack_with_pull) < 0) {
			printf("No response\n");
			optparse_del(parser);
			return SLASH_EIO;
		}
		param_print(param, -1, NULL, 0, 2, time_now.tv_sec);
	}


    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_completer(set, cmd_set, param_completer, "<param> <value>", "Set");



static int cmd_add(struct slash *slash) {
	
	int node = slash_dfl_node;
	char * include_mask_str = NULL;
	char * exclude_mask_str = NULL;
	int force = false;

    optparse_t * parser = optparse_new("cmd add", "<name>[offset] [value]");
    optparse_add_help(parser);
    optparse_add_int(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
	optparse_add_string(parser, 'm', "imask", "MASK", &include_mask_str, "Include mask (param letters) (used for get with wildcard)");
	optparse_add_string(parser, 'e', "emask", "MASK", &exclude_mask_str, "Exclude mask (param letters) (used for get with wildcard)");
    optparse_add_set(parser, 'f', "force", true, &force, "force setting readonly params");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_HWREG;

	if (include_mask_str)
		include_mask = param_maskstr_to_mask(include_mask_str);
	if (exclude_mask_str)
	    exclude_mask = param_maskstr_to_mask(exclude_mask_str);

	/* Check if name is present */
	if (++argi >= slash->argc) {
		printf("missing parameter name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (param_queue.type == PARAM_QUEUE_TYPE_SET) {

		char * name = slash->argv[argi];
		int offset = -1;
		param_t * param = NULL;
		param_slash_parse(name, node, &param, &offset);

		if (param == NULL) {
			printf("%s not found\n", name);
            optparse_del(parser);
			return SLASH_EINVAL;
		}

		if (param->mask & PM_READONLY && !force) {
			printf("--force is required to set a readonly parameter\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}

		/* Check if Value is present */
		if (++argi >= slash->argc) {
			printf("missing parameter value\n");
            optparse_del(parser);
			return SLASH_EINVAL;
		}
		
		char valuebuf[128] __attribute__((aligned(16))) = { };
		if (param_str_to_value(param->type, slash->argv[argi], valuebuf) < 0) {
			printf("invalid parameter value\n");
			optparse_del(parser);
			return SLASH_EINVAL;
		}
		/* clear param timestamp so we dont set timestamp flag when serialized*/
		*param->timestamp = 0;

		if (param_queue_add(&param_queue, param, offset, valuebuf) < 0)
			printf("Queue full\n");

	}

	if (param_queue.type == PARAM_QUEUE_TYPE_GET) {

		char * name = slash->argv[argi];
		int offset = -1;
		param_t * param = NULL;

		/* Go through the list of parameters */
		param_list_iterator i = {};
		while ((param = param_list_iterate(&i)) != NULL) {

			/* Name match (with wildcard) */
			if (strmatch(param->name, name, strlen(param->name), strlen(name)) == 0) {
				continue;
			}

			/* Node match */
			if (param->node != node) {
				continue;
			}

			if (param->mask & exclude_mask) {
				continue;
			}

			if ((param->mask & include_mask) == 0) {
				continue;
			} 

			/* Queue */
			if (param_queue_add(&param_queue, param, offset, NULL) < 0)
				printf("Queue full\n");
			continue;	
			
		}

	}

	param_queue_print(&param_queue);
    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub_completer(cmd, add, cmd_add, param_completer, "<param>[offset] [value]", "Add a new parameter to a command");

static int cmd_run(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	unsigned int hwid = 0;
	int ack_with_pull = true;

    optparse_t * parser = optparse_new("run", "");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_unsigned(parser, 's', "server", "NUM", 0, &server, "server to push parameters to (default = <env>))");
	optparse_add_unsigned(parser, 'h', "hwid", "NUM", 16, &hwid, "include hardware id filter (default = off)");
	optparse_add_set(parser, 'a', "no_ack_push", 0, &ack_with_pull, "Disable ack with param push queue");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (param_queue.type == PARAM_QUEUE_TYPE_SET) {

		csp_timestamp_t time_now;
		csp_clock_get_time(&time_now);
		if (param_push_queue(&param_queue, 0, server, timeout, hwid, ack_with_pull) < 0) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
		param_queue_print_params(&param_queue, time_now.tv_sec);

	}

	if (param_queue.type == PARAM_QUEUE_TYPE_GET) {
		if (param_pull_queue(&param_queue, CSP_PRIO_HIGH, 1, server, timeout)) {
			printf("No response\n");
            optparse_del(parser);
			return SLASH_EIO;
		}
	}


    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, run, cmd_run, "", NULL);

static int cmd_pull(struct slash *slash) {

	unsigned int timeout = slash_dfl_timeout;
	unsigned int server = slash_dfl_node;
	char * include_mask_str = NULL;
	char * exclude_mask_str = NULL;
	int paramver = 2;

    optparse_t * parser = optparse_new("pull", "");
    optparse_add_help(parser);
	optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout in milliseconds (default = <env>)");
	optparse_add_unsigned(parser, 's', "server", "NUM", 0, &server, "server to pull parameters from (default = <env>))");
	optparse_add_string(parser, 'm', "imask", "MASK", &include_mask_str, "Include mask (param letters)");
	optparse_add_string(parser, 'e', "emask", "MASK", &exclude_mask_str, "Exclude mask (param letters)");
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

	if (include_mask_str)
		include_mask = param_maskstr_to_mask(include_mask_str);
	if (exclude_mask_str)
	    exclude_mask = param_maskstr_to_mask(exclude_mask_str);

	if (param_pull_all(CSP_PRIO_HIGH, 1, server, include_mask, exclude_mask, timeout, paramver)) {
		printf("No response\n");
        optparse_del(parser);
		return SLASH_EIO;
	}

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command(pull, cmd_pull, "", "Pull all metrics");

static int cmd_new(struct slash *slash) {

	int paramver = 2;
	char *name = NULL;

    optparse_t * parser = optparse_new("cmd new", "<get/set> <cmd name>");
    optparse_add_help(parser);
    optparse_add_int(parser, 'v', "paramver", "NUM", 0, &paramver, "parameter system version (default = 2)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	if (++argi >= slash->argc) {
		printf("Must specify 'get' or 'set'\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Set/get */
	if (strncmp(slash->argv[argi], "get", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_GET;
	} else if (strncmp(slash->argv[argi], "set", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_SET;
	} else {
		printf("Must specify 'get' or 'set'\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	if (++argi >= slash->argc) {
		printf("Must specify a command name\n");
        optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Command name */
	name = slash->argv[argi];
	strncpy(param_queue.name, name, sizeof(param_queue.name)-1);  // -1 to fit NULL byte

	csp_timestamp_t time_now;
	csp_clock_get_time(&time_now);
	param_queue.used = 0;
	param_queue.version = paramver;
	param_queue.last_timestamp = 0;
	param_queue.client_timestamp = time_now.tv_sec;

	printf("Initialized new command: %s\n", name);

    optparse_del(parser);
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, new, cmd_new, "<get/set> <cmd name>", "Create a new command");


static int cmd_done(struct slash *slash) {
	param_queue.type = PARAM_QUEUE_TYPE_EMPTY;
	return SLASH_SUCCESS;
}
slash_command_sub(cmd, done, cmd_done, "", "Exit cmd edit mode");


static int cmd_print(struct slash *slash) {
	if (param_queue.type == PARAM_QUEUE_TYPE_EMPTY) {
		printf("No active command\n");
	} else {
		printf("Current command size: %d bytes\n", param_queue.used);
		param_queue_print(&param_queue);
	}
	return SLASH_SUCCESS;
}
slash_command(cmd, cmd_print, NULL, "Show current command");
