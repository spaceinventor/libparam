#pragma once

#include <stdint.h>

#include <csp/csp_types.h>
#include <sc/sc.h>
#include <vmem/vmem.h>

void sc_cmd_upload(csp_packet_t * packet);
void sc_cmd_list(csp_packet_t * packet);
void sc_cmd_download(csp_packet_t * packet);
void sc_cmd_execute(csp_packet_t * packet);
void sc_remove(csp_packet_t * packet);

void sc_sch_push(csp_packet_t * packet);
void sc_sch_command(csp_packet_t * packet);

void sc_tick(csp_timestamp_t time_v, uint32_t periodicity_ms);
void sc_init();

/* These variables are defined by projects using this module */
extern vmem_t vmem_sc_cmd_hash;
extern vmem_t vmem_sc_cmd_store;
extern vmem_t vmem_sc_sch_hash;
extern vmem_t vmem_sc_sch_store;

#define SC_CMD_NUM_ELEMENTS 0x80
#define SC_SCH_NUM_ELEMENTS 0x80
#define SC_CMD_BLOCK_SIZE 0x200
#define SC_SCH_BLOCK_SIZE 0x100
