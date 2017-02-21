/*
 * param_log.c
 *
 *  Created on: Jan 4, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <param/param.h>
#include <param_config.h>
#include "param_string.h"
#include "param_log.h"

#include <vmem/vmem.h>
#include <vmem/vmem_fram.h>
#include <vmem_config.h>

#include <csp/csp.h>

#include <param/param_log.h>

VMEM_DEFINE_FRAM(log, "log", VMEM_CONF_LOG_FRAM, VMEM_CONF_LOG_SIZE, VMEM_CONF_LOG_ADDR)

PARAM_DEFINE_STATIC_VMEM(PARAM_LOG_OFFSET + 2, logPages, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", log, 0x08, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAM_LOG_OFFSET + 3, logIn, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", log, 0x0C, NULL);
PARAM_DEFINE_STATIC_VMEM(PARAM_LOG_OFFSET + 4, logOut, PARAM_TYPE_UINT32, -1, 0, UINT32_MAX, PARAM_READONLY_FALSE, NULL, "", log, 0x10, NULL);

static vmem_t * log_vmem;
static int log_page_size;

void param_log(param_t * param, void * new_value, uint32_t timestamp) {

	if (param->log == NULL)
		return;

	char new_value_str[41] = {};
	param_var_str(param->type, param->size, new_value, new_value_str, 40);

	char old_value_str[41] = {};
	param_value_str(param, old_value_str, 40);
	printf("param_log: %s %s => %s (%p[%u])\n", param->name, old_value_str, new_value_str, param->log->phys_addr, param->log->phys_len);

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

}

#include <wdt.h>
#include <FreeRTOS.h>
#include <task.h>

void param_log_init(vmem_t * _log_vmem, int _log_page_size) {
	log_vmem = _log_vmem;
	log_page_size = _log_page_size;

	uint8_t pagebuf[log_page_size];
	unsigned int tstart = xTaskGetTickCount();

	printf("Start %u\n", tstart);

	/** Scan the memory for pages */
	int i = 0;
	for (void *page = log_vmem->vaddr; page < log_vmem->vaddr + log_vmem->size; page += log_page_size) {
		vmem_memcpy(pagebuf, page, log_page_size);
		char pagename[20];
		sprintf(pagename, "page %u", i++);
		//csp_hex_dump(pagename, pagebuf, log_page_size);
		//puts(pagename);
		wdt_restart(WDT);
	}
	printf("Duration %u\n", (unsigned int) xTaskGetTickCount() - tstart);
}
