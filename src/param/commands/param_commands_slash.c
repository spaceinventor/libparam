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

//#include "param_scheduler.h"
#include "param_commands_client.h"
#include "../param_slash.h"

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
		for (int i = 0; i < strlen(slash->argv[2]); i++) {
			name[i] = slash->argv[2][i];
		}
		name[strlen(slash->argv[2])] = '\0';
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
	unsigned int timeout = 100;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
	if (slash->argc >= 3) {
		if (strlen(slash->argv[2]) > 13) {
			return SLASH_EUSAGE;
		}
		for (int i = 0; i < strlen(slash->argv[2]); i++) {
			name[i] = slash->argv[2][i];
		}
		name[strlen(slash->argv[2])] = '\0';
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