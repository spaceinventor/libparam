/*
 * param_commands_slash.c
 *
 *  Created on: Sep 22, 2021
 *      Author: Mads
 */

#include "param_commands_slash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <csp/csp.h>

#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>
#include <param/param_server.h>
#include <param/param_queue.h>

#include "param_commands_client.h"
#include "../param_slash.h"

static void parse_name(char out[], char in[]) {
	for (int i = 0; i < strlen(in); i++) {
			out[i] = in[i];
		}
	out[strlen(in)] = '\0';
}

static int cmd_command_push(struct slash *slash) {
    unsigned int server = 0;
	char name[14] = {0};
	unsigned int timeout = 100;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
	if (slash->argc >= 3) {
		if (strlen(slash->argv[2]) > 13) {
			return SLASH_EUSAGE;
		}
		parse_name(name, slash->argv[2]);
	}
    if (slash->argc >= 4) {
        timeout = atoi(slash->argv[3]);
	}

	if (param_command_push(&param_queue_set, 1, server, name, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(command, push, cmd_command_push, "<server> <name> [timeout]", NULL);

static int cmd_command_show(struct slash *slash) {
    unsigned int server = 0;
	char name[14] = {0};
	unsigned int timeout = 200;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
	if (slash->argc >= 3) {
		if (strlen(slash->argv[2]) > 13) {
			return SLASH_EUSAGE;
		}
		parse_name(name, slash->argv[2]);
	}
    if (slash->argc >= 4) {
        timeout = atoi(slash->argv[3]);
	}

	if (param_command_show(server, 1, name, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(command, show, cmd_command_show, "<server> <name> [timeout]", NULL);

static int cmd_command_list(struct slash *slash) {
    unsigned int server = 0;
	unsigned int timeout = 200;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3) {
        timeout = atoi(slash->argv[2]);
	}

	if (param_command_list(server, 1, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(command, list, cmd_command_list, "<server> [timeout]", NULL);

static int cmd_command_rm(struct slash *slash) {
    unsigned int server = 0;
    char name[14] = {0};
	unsigned int timeout = 200;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3) {
		if (strlen(slash->argv[2]) > 13) {
			return SLASH_EUSAGE;
		}
		parse_name(name, slash->argv[2]);
	}
    if (slash->argc >= 4){
        timeout = atoi(slash->argv[3]);
	}

	int rm_all = 0;
	int count = 0;
	char rmallcmds[] = "RMALLCMDS";
	if (strlen(name) == strlen(rmallcmds)) {
		for (int i = 0; i < strlen(name); i++) {
			if (name[i] == rmallcmds[i]) {
				count++;
			} else {
				break;
			}
		}
		if (count == strlen(rmallcmds)) {
			rm_all = 1;
		}
	}


	if (rm_all == 1) {
		if (param_command_rm_all(server, 1, name, timeout) < 0) {
			printf("No response\n");
			return SLASH_EIO;
		}
	} else if (rm_all == 0) {
		if (param_command_rm(server, 1, name, timeout) < 0) {
			printf("No response\n");
			return SLASH_EIO;
		}
	}

	return SLASH_SUCCESS;
}
slash_command_sub(command, rm, cmd_command_rm, "<server> <name> [timeout]", NULL);