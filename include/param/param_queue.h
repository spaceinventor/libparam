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
#ifdef __cplusplus
extern "C" {
#endif

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
	csp_timestamp_t last_timestamp;
	csp_timestamp_t client_timestamp;
} param_queue_t;

/**
 * @brief Control the amount of prints that the param_queue_* functions will output
 */
extern uint32_t param_queue_dbg_level;

void param_queue_init(param_queue_t * queue, void * buffer, int buffer_size, int used, param_queue_type_e type, int version);

int param_queue_add(param_queue_t *queue, param_t *param, int offset, void *value);

/**
 * @brief 						Applies the content of a queue to memory.
 * @param queue[in]				Pointer to queue
 * @param from[in]              If from is set and the node is 0, it will be set to from
 * @return 						0 OK, -1 ERROR
 */
int param_queue_apply(param_queue_t *queue, int from);

void param_queue_print(param_queue_t *queue);
void param_queue_print_local(param_queue_t *queue);
void param_queue_print_params(param_queue_t *queue, uint32_t ref_timestamp);

typedef int (*param_queue_callback_f)(void * context, param_queue_t *queue, param_t * param, int offset, void *reader);
int param_queue_foreach(param_queue_t *queue, param_queue_callback_f callback, void * context);


void param_deserialize_id(mpack_reader_t *reader, int *id, int *node, csp_timestamp_t *timestamp, int *offset, param_queue_t *queue);

#define PARAM_QUEUE_FOREACH(param, reader, queue, offset) \
	mpack_reader_t reader; \
	mpack_reader_init_data(&reader, queue->buffer, queue->used); \
	while(reader.data < reader.end) { \
		int id, node, offset = -1; \
		csp_timestamp_t timestamp = { .tv_sec = 0, .tv_nsec = 0 }; \
		param_deserialize_id(&reader, &id, &node, &timestamp, &offset, queue); \
		param_t * param = param_list_find_id(node, id); \


#ifdef __cplusplus
}
#endif

#endif /* LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_ */
