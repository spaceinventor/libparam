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
	char name[36];
} __attribute__((packed)) param_transfer_t;

typedef struct {
    uint16_t id;
    uint16_t node;
    uint8_t type;
    uint8_t size;
    uint32_t mask;
    char name[36];
} __attribute__((packed)) param_transfer2_t;

typedef struct {
    uint16_t id;
    uint16_t node;
    uint8_t type;
    uint8_t size;
    uint32_t mask;
    char name[36];
    uint8_t storage_type;
    char unit[10];
    char help[150];
} __attribute__((packed)) param_transfer3_t;

#endif /* LIB_PARAM_SRC_PARAM_PARAM_LIST_H_ */
