/*
 * param_queue.h
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_
#define LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_

#include <param/param.h>

struct param_queue_s;
typedef struct param_queue_s param_queue_t;

param_queue_t * param_queue_create(void);
void param_queue_destroy(param_queue_t *queue);

void param_queue_print(param_queue_t *queue);
int param_queue_add(param_queue_t *queue, param_t * param, void * value);




#endif /* LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_ */
