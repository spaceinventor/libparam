#pragma once

#include <stdint.h>

#include <csp/csp_types.h>
#include <sc/sc.h>

int sc_cmd_upload_client(param_queue_t* param_queue, uint16_t server, unsigned int timeout);
int sc_cmd_download_client(param_hash_t hash, uint16_t server, unsigned int timeout);
int sc_cmd_list_client(uint16_t server, unsigned int timeout);
int sc_cmd_remove_client(param_hash_t hash, uint16_t server, unsigned int timeout);
int sc_sch_push_client(param_queue_t* queue, uint32_t time, uint16_t latency_buffer_s, uint16_t server, unsigned int timeout);
int sc_sch_cmd_client(param_hash_t cmd_hash, uint32_t time, uint16_t latency_buffer_s, uint16_t server, unsigned int timeout);
