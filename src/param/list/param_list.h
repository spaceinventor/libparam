/*
 * param_list.h
 *
 *  Created on: Jan 13, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_LIST_H_
#define LIB_PARAM_SRC_PARAM_PARAM_LIST_H_

typedef struct {
	uint16_t id;
	uint8_t type;
	uint8_t size;
	uint32_t mask;
	char name[];
} __attribute__((packed)) param_transfer_t;

#endif /* LIB_PARAM_SRC_PARAM_PARAM_LIST_H_ */
