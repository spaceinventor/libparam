/*
 * param_group_beacon.c
 *
 *  Created on: Jan 16, 2017
 *      Author: johan
 */

#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_clock.h>
#include <csp/arch/csp_time.h>

#include <param/param_group.h>

#define DEFAULT_SLEEP 10000 // [ms]
#define SLEEP_MIN 100 // [ms]

csp_thread_return_t param_group_beacon_task(void *pvParameters) {

	while(1) {

		int sleep_min = DEFAULT_SLEEP;
		uint32_t now = csp_get_ms();

		/* Loop over groups */
		param_group_t * group;
		param_group_iterator i = {};
		while((group = param_group_iterate(&i)) != NULL) {

			if (group->interval == 0)
				continue;

			printf("Group %s\n", group->name);

			unsigned int last_beacon_slot = group->last_beacon / group->interval;
			unsigned int current_slot = now / group->interval;

			if (current_slot > last_beacon_slot) {
				printf("sending beacon %s last %u now %u, slot %u < %u\n", group->name, (unsigned int) group->last_beacon, (unsigned int) now, last_beacon_slot, current_slot);
				group->last_beacon = now;
				param_group_copy(group, group->node);
			}

			int sleep = (current_slot * group->interval) + group->interval - now;
			if (sleep < SLEEP_MIN)
				sleep = SLEEP_MIN;
			if (sleep < sleep_min)
				sleep_min = sleep;

		}

		printf("Sleep %u\n", sleep_min);

		csp_sleep_ms(sleep_min);

	}

}
