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

#include "param_collector_config.h"

csp_thread_return_t param_collector_task(void *pvParameters) {

	param_collector_init();

	while(1) {

		csp_sleep_ms(100);

		for(int i = 0; i < 16; i++) {
			if (param_collector_config[i].node == 0)
				break;

			if (csp_get_ms() < param_collector_config[i].last_time + param_collector_config[i].interval) {
				continue;
			}

			param_collector_config[i].last_time = csp_get_ms();

			param_pull_all(0, param_collector_config[i].node, param_collector_config[i].mask, 1000);
		}

	}

}

