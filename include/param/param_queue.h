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

typedef enum {
	PARAM_QUEUE_TYPE_GET,
	PARAM_QUEUE_TYPE_SET,
} param_queue_type_e;

struct param_queue_s {
	char *intbuffer;
	char *extbuffer;
	mpack_writer_t writer;
	param_queue_type_e type;
};

typedef struct param_queue_s param_queue_t;

param_queue_t * param_queue_create(void * buffer, int buffer_length, param_queue_type_e type);
void param_queue_destroy(param_queue_t *queue);

void param_queue_print(param_queue_t *queue);
int param_queue_push(param_queue_t *queue, param_t * param, void * value);
int param_queue_pop(param_queue_t *queue, param_t * param, void * value);




#endif /* LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_ */
