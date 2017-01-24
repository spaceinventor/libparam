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

#define DEFAULT_SLEEP 10000 // [ms]
#define SLEEP_MIN 100 // [ms]
#define MAX_NODES 10
#define MAX_PARAMS 100

#define BASIS_FILTER() \
	/* Basis filter */ \
	if (param->refresh == 0) \
		continue; \
	if (param->storage_type != PARAM_STORAGE_REMOTE) \
		continue; \

csp_thread_return_t param_collector_task(void *pvParameters)
{
	while(1) {

		uint32_t now = csp_get_ms();

		/* Get list of unique nodes */
		uint8_t nodes[MAX_NODES];
		uint8_t nodes_count = 0;

		param_t * param;
		param_list_iterator i = {};
		while ((param = param_list_iterate(&i)) != NULL) {

			/* Basis filter */
			BASIS_FILTER();

			/* Search for node in list */
			for (int i = 0; i < nodes_count; i++) {
				if (nodes[i] == param->node)
					continue;
			}

			/* If not found, add to list */
			if (nodes_count < MAX_NODES)
				nodes[nodes_count++] = param->node;

			continue;

		}

		/* If no nodes was found, return now */
		if (nodes_count == 0)
			continue;

		int sleep_min = DEFAULT_SLEEP;

		for (int i = 0; i < nodes_count; i++) {

			param_t * params[MAX_PARAMS];
			int params_count = 0;

			param_t * param;
			param_list_iterator iterator = {};
			while ((param = param_list_iterate(&iterator)) != NULL) {

				/* Basis filter */
				BASIS_FILTER();

				/* Node filter */
				if (param->node != nodes[i])
					continue;

				unsigned int updated_slot = param->value_updated / param->refresh;
				unsigned int current_slot = now / param->refresh;

				if (current_slot > updated_slot) {
					//printf("getting param %s last %u refresh %u, slot %u < %u\n", param->name, (unsigned int) param->value_updated, param->refresh, updated_slot, current_slot);
					if (params_count < MAX_PARAMS)
						params[params_count++] = param;
				}

				int sleep = (current_slot * param->refresh) + param->refresh - now;
				if (sleep < SLEEP_MIN)
					sleep = SLEEP_MIN;
				if (sleep < sleep_min)
					sleep_min = sleep;

				//printf("Sleep %d sleep_min %d\n", sleep, sleep_min);

			}

			if (params_count == 0)
				continue;

			param_pull(params, params_count, 0, nodes[i], 100);

		}

		csp_sleep_ms(sleep_min)

	}
}


