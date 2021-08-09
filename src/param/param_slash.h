/*
 * param_slash.h
 *
 *  Created on: Jan 2, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_SLASH_H_
#define LIB_PARAM_SRC_PARAM_PARAM_SLASH_H_

#include <param/param.h>
#include <param/param_queue.h>

extern param_queue_t param_queue_set;
extern param_queue_t param_queue_get;

void param_slash_parse(char * arg, param_t **param, int *node, int *host, int *offset);


#endif /* LIB_PARAM_SRC_PARAM_PARAM_SLASH_H_ */
