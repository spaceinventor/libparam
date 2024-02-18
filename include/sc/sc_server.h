#pragma once

#include <stdint.h>

#include <csp/csp_types.h>
#include <sc/sc.h>

void sc_cmd_upload(csp_packet_t * packet);
void sc_cmd_list(csp_packet_t * packet);
void sc_cmd_download(csp_packet_t * packet);
void sc_cmd_execute(csp_packet_t * packet);
void sc_remove(csp_packet_t * packet);

void sc_sch_push(csp_packet_t * packet);
void sc_sch_command(csp_packet_t * packet);

void sc_tick(csp_timestamp_t time_v, uint32_t periodicity_ms);
void sc_init();
