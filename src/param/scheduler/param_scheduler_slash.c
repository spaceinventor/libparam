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

//#include "param_scheduler.h"
#include "param_scheduler_client.h"
#include "../param_slash.h"

static int cmd_schedule_push(struct slash *slash) {
    unsigned int server = 0;
	unsigned int host = 0;
    unsigned int time = 0;
	unsigned int latency_buffer = 0;
	unsigned int timeout = 100;

	if (slash->argc < 5)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
	if (slash->argc >= 3)
        host = atoi(slash->argv[2]);
    if (slash->argc >= 4)
        time = atoi(slash->argv[3]);
    if (slash->argc >= 5)
        latency_buffer = atoi(slash->argv[4]);
    if (slash->argc >= 6) {
        timeout = atoi(slash->argv[5]);
	}

	if (param_schedule_push(&param_queue_set, 1, server, host, time, latency_buffer, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, push, cmd_schedule_push, "<server> <host> <time> <latency buffer> [timeout]", NULL);

#if 0
static int cmd_schedule_pull(struct slash *slash) {
    unsigned int server = 0;
    unsigned int time = 0;
	unsigned int host = 0;
	unsigned int timeout = 100;

	if (slash->argc < 4)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
	if (slash->argc >= 3)
        host = atoi(slash->argv[2]);
    if (slash->argc >= 4)
        time = atoi(slash->argv[3]);
    if (slash->argc >= 5){
        timeout = atoi(slash->argv[4]);
	}

	if (param_schedule_push(&param_queue_get, 1, server, host, time, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, pull, cmd_schedule_pull, "<server> <host> <time> [timeout]", NULL);
#endif

static int cmd_schedule_list(struct slash *slash) {
    unsigned int server = 0;
	unsigned int timeout = 100;

	if (slash->argc < 2)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3){
        timeout = atoi(slash->argv[2]);
	}

	if (param_list_schedule(server, 1, timeout) < 0) {
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
    if (slash->argc >= 4){
		timeout = atoi(slash->argv[3]);
	}

	if (param_show_schedule(server, 1, id, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, show, cmd_schedule_show, "<server> <id> [timeout]", NULL);

static int cmd_schedule_rm(struct slash *slash) {
    unsigned int server = 0;
    int id = 0;
	unsigned int timeout = 100;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3)
        id = atoi(slash->argv[2]);
    if (slash->argc >= 4){
        timeout = atoi(slash->argv[3]);
	}
	if (id < -1)
		return SLASH_EUSAGE;

	if (id == -1) {
		if (param_rm_all_schedule(server, 1, timeout) < 0) {
			printf("No response\n");
			return SLASH_EIO;
		}
	} else if (id >= 0) {
		if (param_rm_schedule(server, 1, id, timeout) < 0) {
			printf("No response\n");
			return SLASH_EIO;
		}
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, rm, cmd_schedule_rm, "<server> <id> [timeout]", NULL);

static int cmd_schedule_reset(struct slash *slash) {
    unsigned int server = 0;
	uint16_t last_id = 0;
	unsigned int timeout = 100;

	if (slash->argc < 3)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
    if (slash->argc >= 3)
        last_id = atoi(slash->argv[4]);
    if (slash->argc >= 4){
        timeout = atoi(slash->argv[3]);
	}

	if (param_reset_scheduler(server, last_id, 1, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, reset, cmd_schedule_reset, "<server> <last id> [timeout]", NULL);

static void parse_name(char out[], char in[]) {
	for (int i = 0; i < strlen(in); i++) {
			out[i] = in[i];
		}
	out[strlen(in)] = '\0';
}

static int cmd_schedule_command(struct slash *slash) {
    unsigned int server = 0;
	char name[14] = {0};
	unsigned int host = 0;
    unsigned int time = 0;
	unsigned int latency_buffer = 0;
	unsigned int timeout = 100;

	if (slash->argc < 6)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
	if (slash->argc >= 3) {
		if (strlen(slash->argv[2]) > 13) {
			return SLASH_EUSAGE;
		}
		parse_name(name, slash->argv[2]);
	}
	if (slash->argc >= 4)
        host = atoi(slash->argv[3]);
    if (slash->argc >= 5) {
        time = atoi(slash->argv[4]);
	}
	if (slash->argc >= 6)
        latency_buffer = atoi(slash->argv[5]);
    if (slash->argc >= 7) {
        timeout = atoi(slash->argv[6]);
	}

	if (param_schedule_command(1, server, name, host, time, latency_buffer, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, cmd, cmd_schedule_command, "<server> <name> <host> <time> <latency buffer> [timeout]", NULL);
