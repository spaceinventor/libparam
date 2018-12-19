/*
 * param_collector_config.h
 *
 *  Created on: Aug 30, 2018
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_COLLECTOR_PARAM_COLLECTOR_CONFIG_H_
#define LIB_PARAM_SRC_PARAM_COLLECTOR_PARAM_COLLECTOR_CONFIG_H_

#include <param/param.h>

struct param_collector_config_s {
	uint8_t node;
	uint32_t interval;
	uint32_t mask;
	uint32_t last_time;
};

extern struct param_collector_config_s param_collector_config[];

extern param_t col_run;
extern param_t col_verbose;
extern param_t col_cnfstr;

void param_collector_init(void);

#endif /* LIB_PARAM_SRC_PARAM_COLLECTOR_PARAM_COLLECTOR_CONFIG_H_ */
