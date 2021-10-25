/*
 * objstore.h
 *
 *  Created on: Sep 10, 2021
 *      Author: Mads
 */

#pragma once

#include <stdint.h>

#include <vmem/vmem.h>

typedef enum {
    OBJ_TYPE_SCHEDULE = 1,
    OBJ_TYPE_COMMAND = 2,
    OBJ_TYPE_SCHEDULER_META = 3,
    OBJ_TYPE_COMMANDS_META = 4,
    OBJ_TYPE_HK_QUEUE = 5,
    OBJ_TYPE_HK_INDEX = 6,
} objstore_type_e;

#define NUM_OBJ_TYPES 4

#define OBJ_HEADER_LENGTH 7
#define OBJ_CHKSUM_LENGTH 1

typedef struct objstore_idx_s objstore_idx_t;

struct objstore_idx_s {
    objstore_type_e type;
    uint16_t offset;
    uint8_t length;
    objstore_idx_t * ptr_prev;
    objstore_idx_t * ptr_next;
};
typedef struct objstore_idx_s objstore_idx_t;

typedef int (*objstore_scan_callback_f)(vmem_t * vmem, int offset, int verbose, void * ctx);

int objstore_scan(vmem_t * vmem, objstore_scan_callback_f callback, int verbose, void * ctx);
int objstore_alloc(vmem_t * vmem, int length, int verbose);

void objstore_write_obj(vmem_t * vmem, int offset, uint8_t type, uint16_t length, void * data);
void objstore_write_data(vmem_t * vmem, int obj_offset, int data_offset, int data_length, void * data);
int objstore_rm_obj(vmem_t * vmem, int offset, int verbose);

int objstore_read_obj_type(vmem_t * vmem, int offset);
int objstore_read_obj_length(vmem_t * vmem, int offset);
int objstore_read_obj(vmem_t * vmem, int offset, void * data_buf, int verbose);