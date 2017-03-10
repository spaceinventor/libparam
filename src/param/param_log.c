/*
 * param_log.c
 *
 *  Created on: Jan 4, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <param/param.h>
#include <param_config.h>
#include "param_serializer.h"
#include "param_string.h"
#include "param_log.h"

#include <mpack/mpack.h>

#include <vmem/vmem.h>
#include <vmem/vmem_fram.h>
#include <vmem_config.h>

#include <csp/csp.h>

#include <param/param_log.h>

VMEM_DEFINE_FRAM(log, "log", VMEM_CONF_LOG_FRAM, VMEM_CONF_LOG_SIZE, VMEM_CONF_LOG_ADDR)

PARAM_DEFINE_STATIC_VMEM(PARAM_LOG_OFFSET + 2, pl_cnt, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", log, 0x08, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAM_LOG_OFFSET + 3, pl_in, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", log, 0x0C, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAM_LOG_OFFSET + 4, pl_out, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", log, 0x10, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAM_LOG_OFFSET + 5, plid_next, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", log, 0x14, NULL);

static vmem_t * log_vmem;
static int log_page_size;

static mpack_writer_t writer;
static param_log_page_t * workpage = NULL;

static void param_log_setup_workpage(void) {

	if (workpage == NULL)
		workpage = malloc(log_page_size);

	mpack_writer_init(&writer, (char *) workpage->data, log_page_size - offsetof(param_log_page_t, data));

}

static void param_log_flush_workpage(void) {

	int pageid = param_get_uint32(&pl_in);
	void * pageaddr = log_vmem->vaddr + pageid * log_page_size;

	/* Rollover */
	if (pageaddr >= log_vmem->vaddr + log_vmem->size) {
		pageid = 0;
		pageaddr = 0;
	}

	workpage->plid = param_get_uint32(&plid_next);

	printf("Flushing to page %u at vaddr %p\n", (unsigned int) pageid, pageaddr);
	vmem_memcpy(pageaddr, workpage, log_page_size);

	/* Increment plid and logIn */
	param_set_uint32(&plid_next, param_get_uint32(&plid_next) + 1);
	param_set_uint32(&pl_in, param_get_uint32(&pl_in) + 1);
	param_set_uint32(&pl_cnt, param_get_uint32(&pl_cnt) + 1);

	param_log_setup_workpage();

}


void param_log(param_t * param, void * new_value, uint32_t timestamp) {

	if (param->log == NULL)
		return;

	char new_value_str[41] = {};
	param_var_str(param->type, param->size, new_value, new_value_str, 40);

	char old_value_str[41] = {};
	param_value_str(param, old_value_str, 40);
	printf("param_log: %s %s => %s\n", param->name, old_value_str, new_value_str);

	/* Compare values */
	char old_value[param_size(param)];
	param_get_data(param, old_value, param_size(param));

	//csp_hex_dump("old", old_value, param_size(param));
	//csp_hex_dump("new", new_value, param_size(param));

	if (memcmp(old_value, new_value, param_size(param)) == 0) {
		printf("No change\n");
		return;
	}

	printf("Change detected, logging now\n");

	mpack_write_u32(&writer, new_value);

	csp_hex_dump("workpage", workpage, log_page_size);

	if (mpack_writer_buffer_used(&writer) > 32)
		param_log_flush_workpage();

}

#include <wdt.h>
#include <FreeRTOS.h>
#include <task.h>

void param_log_init(vmem_t * _log_vmem, int _log_page_size) {
	log_vmem = _log_vmem;
	log_page_size = _log_page_size;

	/* Init working page */
	param_log_setup_workpage();

	/* Time scanning */
	unsigned int tstart = xTaskGetTickCount();
	printf("Start %u\n", tstart);

	/* Scan the memory for pages */
	uint32_t plid_low = UINT32_MAX;
	uint32_t plid_low_at = 0;
	uint32_t plid_high = 0;
	uint32_t plid_high_at = 0;
	uint32_t plid_valid_cnt = 0;

	param_log_page_t * scanbuf = malloc(log_page_size);
	unsigned int i = 0;
	for (void *page = log_vmem->vaddr; page < log_vmem->vaddr + log_vmem->size; page += log_page_size, i++) {
		wdt_restart(WDT);
		vmem_memcpy(scanbuf, page, log_page_size);
		if (scanbuf->plid == 0xFFFFFFFF)
			break;
		if (scanbuf->plid > plid_high) {
			plid_high = scanbuf->plid;
			plid_high_at = i;
		}
		if (scanbuf->plid < plid_low) {
			plid_low = scanbuf->plid;
			plid_low_at = i;
		}
		plid_valid_cnt++;
		printf("Page %u@%u\n", (unsigned int) scanbuf->plid, i);

		/* Deserialize */
		//int j = 0;
		//j += param_deserialize_chunk_param_and_value(csp_get_address(), 0, 1, scanbuf->data);


		//mpack_tree_t tree;
		//mpack_tree_init(&tree, (char *) scanbuf->data, log_page_size - offsetof(param_log_page_t, data));
		//mpack_node_t * node = mpack_tree_root(&tree);
		mpack_print((char *) scanbuf->data, log_page_size - offsetof(param_log_page_t, data));



	}
	printf("plid low %u@%u, high %u@%u\n", (unsigned int) plid_low, (unsigned int) plid_low_at, (unsigned int) plid_high, (unsigned int) plid_high_at);
	free(scanbuf);

	param_set_uint32(&pl_cnt, plid_valid_cnt);
	param_set_uint32(&pl_out, plid_low_at);
	param_set_uint32(&pl_in, plid_high_at);

	/* End timing */
	printf("Duration %u\n", (unsigned int) xTaskGetTickCount() - tstart);

}
