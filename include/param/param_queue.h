/*
 * param_queue.h
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_
#define LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_

#include <param/param.h>
#include <param/param_list.h>
#include <mpack/mpack.h>

typedef enum {
	PARAM_QUEUE_TYPE_GET,
	PARAM_QUEUE_TYPE_SET,
	PARAM_QUEUE_TYPE_EMPTY,
} param_queue_type_e;

typedef struct param_queue_s {
	char *buffer;
	uint16_t buffer_size;
	uint16_t used;
	uint8_t version;
	uint8_t type;
	char name[20];

	/* State used by serializer */
	uint16_t last_node;
	uint32_t last_timestamp;
} param_queue_t;

void param_queue_init(param_queue_t * queue, void * buffer, int buffer_size, int used, param_queue_type_e type, int version);

int param_queue_add(param_queue_t *queue, param_t *param, int offset, void *value);

/**
 * @brief 						Applies the content of a queue to memory.
 * @param queue[in]				Pointer to queue
 * @param apply_local[in]	 	If the apply local flag is set the node will be set to 0.
 * @param from[in]              If from is set and the node is 0, it will be set to from
 * @return 						0 OK, -1 ERROR
 */
int param_queue_apply(param_queue_t *queue, int apply_local, int from, int store_timestamp);

void param_queue_print(param_queue_t *queue);
void param_queue_print_local(param_queue_t *queue);

typedef int (*param_queue_callback_f)(void * context, param_queue_t *queue, param_t * param, int offset, void *reader);
int param_queue_foreach(param_queue_t *queue, param_queue_callback_f callback, void * context);


void param_deserialize_id(mpack_reader_t *reader, int *id, int *node, long unsigned int *timestamp, int *offset, param_queue_t *queue);

#define PARAM_QUEUE_FOREACH(param, reader, queue, offset) \
	mpack_reader_t reader; \
	mpack_reader_init_data(&reader, queue->buffer, queue->used); \
	while(reader.data < reader.end) { \
		int id, node, offset = -1; \
		long unsigned int timestamp = 0; \
		param_deserialize_id(&reader, &id, &node, &timestamp, &offset, queue); \
		param_t * param = param_list_find_id(node, id); \



#endif /* LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_ */
