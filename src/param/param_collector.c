/*
 * param_collector.c
 *
 *  Created on: Jan 2, 2017
 *      Author: johan
 */

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_clock.h>
#include <csp/arch/csp_time.h>

#include <param/param_collector.h>
#include <param/param_client.h>
#include <param/param_list.h>

csp_thread_return_t param_collector_task(void *pvParameters)
{
	while(1) {

		uint32_t now = csp_get_ms();

		/* Search for next wakeup time */
		int iterator(param_t * param) {
			if (param->refresh == 0)
				return 1;

			if (param->storage_type != PARAM_STORAGE_REMOTE)
				return 1;

			unsigned int updated_slot = param->value_updated / param->refresh;
			unsigned int current_slot = now / param->refresh;

			if (current_slot > updated_slot) {
				printf("getting param %s last %u refresh %u, slot %u < %u\n", param->name, (unsigned int) param->value_updated, param->refresh, updated_slot, current_slot);
				param_pull_single(param, param->node, 100);
			}

			return 1;
		}
		param_list_foreach(iterator);
		vTaskDelay(1000);
	}
}


