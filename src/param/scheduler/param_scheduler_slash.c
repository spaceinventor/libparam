/*
 * param_scheduler_slash.c
 *
 *  Created on: Aug 9, 2021
 *      Author: Mads
 */

#include "param_scheduler_slash.h"

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

#include "param_scheduler.h"
#include "param_scheduler_client.h"
#include "../param_slash.h"

static int cmd_schedule_push(struct slash *slash) {
    unsigned int server = 0;
    unsigned int time = 0;
	unsigned int timeout = 100;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3)
        time = atoi(slash->argv[2]);
    if (slash->argc >= 4)
        timeout = atoi(slash->argv[3]);

	if (param_schedule_push(&param_queue_set, 1, server, time, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, push, cmd_schedule_push, "<server> <time> [timeout]", NULL);

static int cmd_schedule_pull(struct slash *slash) {
    unsigned int server = 0;
    unsigned int time = 0;
	unsigned int timeout = 100;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3)
        time = atoi(slash->argv[2]);
    if (slash->argc >= 4)
        timeout = atoi(slash->argv[3]);

	if (param_schedule_push(&param_queue_get, 1, server, time, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, pull, cmd_schedule_pull, "<server> <time> [timeout]", NULL);

static int cmd_schedule_list(struct slash *slash) {
    unsigned int server = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3)
        timeout = atoi(slash->argv[2]);

    //param_schedule_list_t schedule_list[SCHEDULE_LIST_LENGTH];
	if (param_list_schedule(server, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}


	return SLASH_SUCCESS;
}
slash_command_sub(schedule, list, cmd_schedule_list, "<server> [timeout]", NULL);

static int cmd_schedule_show(struct slash *slash) {
    unsigned int server = 0;
    unsigned int id = 0;
	unsigned int timeout = 100;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3)
        id = atoi(slash->argv[2]);
    if (slash->argc >= 4)
        timeout = atoi(slash->argv[3]);

	if (param_show_schedule(server, id, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, show, cmd_schedule_show, "<server> <id> [timeout]", NULL);