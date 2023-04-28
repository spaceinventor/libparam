/*
 * objstore_slash.c
 *
 *  Created on: Sep 14, 2021
 *      Author: Mads
 */

#include "objstore_slash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <slash/slash.h>

#include <csp/csp.h>

#include <objstore/objstore.h>

#include <vmem/vmem.h>
#include <vmem/vmem_file.h>

#include <param/param_scheduler.h>

#if 0
static int cmd_schedule_push(struct slash *slash) {
    unsigned int server = 0;
    unsigned int time = 0;
	unsigned int host = 0;
	unsigned int timeout = slash_dfl_timeout;

	if (slash->argc < 4)
		return SLASH_EUSAGE;
	if (slash->argc >= 2)
		server = atoi(slash->argv[1]);
	if (slash->argc >= 3)
        host = atoi(slash->argv[2]);
    if (slash->argc >= 4)
        time = atoi(slash->argv[3]);
    if (slash->argc >= 5) {
        timeout = atoi(slash->argv[4]);
	}

	if (param_schedule_push(&param_queue_set, 1, server, host, time, timeout) < 0) {
		printf("No response\n");
		return SLASH_EIO;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(schedule, push, cmd_schedule_push, "<server> <host> <time> [timeout]", NULL);
#endif


VMEM_DEFINE_FILE(slashfile, "slashfile", "schedule.cnf", 0x1000);


static int objstore_scan_callback(vmem_t * vmem, int offset, int verbose, void * ctx) {

    if (verbose) {
        printf("Found sync-word 5C0FFEE1 at offset %u\n", offset);

        uint8_t type, length;

        vmem->read(vmem, offset+4, &type, 1);
        printf(" object type: %u\n", type);

        vmem->read(vmem, offset+5, &length, 1);
        printf(" object length: %u\n", length);
    }

	return 0;

}

static int cmd_objstore_scan(struct slash *slash) {
	if (slash->argc >= 1)
		return SLASH_EUSAGE;

	objstore_scan(&vmem_slashfile, objstore_scan_callback, 1, NULL);

	return SLASH_SUCCESS;
}
slash_command_sub(objstore, scan, cmd_objstore_scan, "", NULL);


static int cmd_objstore_rm_obj(struct slash *slash) {
    int offset = 0;

	if (slash->argc < 1)
		return SLASH_EUSAGE;
    if (slash->argc >= 2) {
        offset = atoi(slash->argv[1]);
	}

	if (objstore_rm_obj(&vmem_slashfile, offset, 1) < 0) {
		return SLASH_ENOENT;
	}

	return SLASH_SUCCESS;
}
slash_command_sub(objstore, rmobj, cmd_objstore_rm_obj, "<offset>", NULL);