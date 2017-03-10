/*
 * param_log.h
 *
 *  Created on: Jan 4, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_LOG_H_
#define LIB_PARAM_SRC_PARAM_PARAM_LOG_H_

typedef struct param_log_page_s {
	uint32_t plid;
	uint32_t timestamp;
	uint8_t data[];
} param_log_page_t;

void param_log(param_t * param, void * new_value, uint32_t timestamp);

#endif /* LIB_PARAM_SRC_PARAM_PARAM_LOG_H_ */
