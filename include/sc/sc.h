#pragma once

#include <stdint.h>
#include <param/param_queue.h>
#include <csp/csp_crc32.h>

/* Each message use the default param header, in which the first byte indicate the msg type, 
   and the second byte indicate if the message ends the transmission 
   All of the following structs can be added multiple times in one message, the message length
   indicates the number of elements */

typedef csp_crc32_t param_hash_t;

typedef enum {
    SC_TYPE_CMD = 1,
    SC_TYPE_SCH = 2,
} sc_type_e;

typedef uint8_t sc_type_t;

/* Request a particular command or schedule to be downloaded, executed or removed from storage 
   PARAM_COMMAND_EXECUTE_REQUEST_V2
   PARAM_COMMAND_DOWNLOAD_REQUEST_V2
   PARAM_COMMAND_REMOVE_REQUEST_V2
   PARAM_SCHEDULE_REMOVE_REQUEST_V2
   */
typedef struct {
    param_hash_t hash;
} param_sc_req_t;

/* Response to command, schedule, removal or execute requests 
   PARAM_COMMAND_UPLOAD_RESPONSE_V2
   PARAM_COMMAND_EXECUTE_RESPONSE_V2
   PARAM_COMMAND_LIST_RESPONSE_V2
   PARAM_SCHEDULE_PUSH_RESPONSE_V2
   PARAM_SCHEDULE_COMMAND_RESPONSE_V2
   */
typedef struct {
    param_hash_t hash;
    int8_t result;
} param_sc_rsp_t;

/* Upload a command queue
   Potentially for storage and/or immediate execution 
   PARAM_COMMAND_UPLOAD_REQUEST_V2
   */
typedef struct {
    param_queue_t param_queue;
    uint8_t execute;
    uint8_t store;
    char param_buffer[];
} param_cmd_upload_t;

/* Response to a download request of a single or all commands 
   PARAM_COMMAND_DOWNLOAD_REQUEST_V2
   */
typedef struct {
    param_queue_t param_queue;
    char param_buffer[];
} param_cmd_download_t;

/* Upload a command schedule, with the command queue included
   PARAM_SCHEDULE_PUSH_REQUEST_V2
   */
typedef struct {
    uint32_t timestamp_s;
    uint32_t latency_buffer_s;
    param_queue_t param_queue;
    char param_buffer[];
} param_sch_push_t;

/* Schedule a command already existing in the target module 
   PARAM_SCHEDULE_COMMAND_REQUEST_V2
   */
typedef struct {
    uint32_t timestamp_s;
    uint32_t latency_buffer_s;
    param_hash_t command_hash;
} param_sch_command_t;
