/*
 * param_queue.h
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_
#define LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_

#include <param/param.h>
#include <mpack/mpack.h>

struct param_queue_s {
	char *buffer;
	mpack_writer_t writer;
};

typedef struct param_queue_s param_queue_t;

param_queue_t * param_queue_create(void * buffer, int buffer_length);
void param_queue_destroy(param_queue_t *queue);

void param_queue_print(param_queue_t *queue);
int param_queue_add(param_queue_t *queue, param_t * param, void * value);




#endif /* LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_ */
