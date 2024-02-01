/*
 * param_collector.c
 *
 *  Created on: Jan 2, 2017
 *      Author: johan
 */

#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <unistd.h>

#include <param/param_collector.h>
#include <param/param_client.h>
#include <param/param_list.h>

#include "param_collector_config.h"

void param_collector_loop(void * param) {

	param_collector_init();

	while(1) {

		usleep(100000);

		for(int i = 0; i < 16; i++) {
			if (param_collector_config[i].node == 0)
				break;

			if (param_get_uint8(&col_run) == 0)
				continue;

			if (csp_get_ms() < param_collector_config[i].last_time + param_collector_config[i].interval) {
				continue;
			}

			param_collector_config[i].last_time = csp_get_ms();

			param_pull_all(CSP_PRIO_NORM, param_get_uint8(&col_verbose), param_collector_config[i].node, param_collector_config[i].mask, 0, 1000, 2);
		}

	}

}

