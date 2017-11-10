/*
 * param_queue.c
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#include <csp/csp.h>
#include <mpack/mpack.h>

#include <param/param.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <param/param_list.h>
#include "param_serializer.h"

param_queue_t * param_queue_create(void * buffer, int buffer_size, int used, param_queue_type_e type) {

	param_queue_t * queue = malloc(sizeof(param_queue_t));

	if (buffer) {
		queue->buffer = buffer;
		queue->buffer_internal = 0;
	} else {
		queue->buffer = malloc(buffer_size);
		queue->buffer_internal = 1;
	}

	queue->buffer_size = buffer_size;
	queue->type = type;
	queue->used = used;

	return queue;

}

void param_queue_destroy(param_queue_t *queue) {
	if (queue->buffer_internal)
		free(queue->buffer);
	free(queue);
}

int param_queue_add(param_queue_t *queue, param_t *param, void *value) {
	mpack_writer_t writer;
	mpack_writer_init(&writer, queue->buffer, queue->buffer_size);
	writer.used = queue->used;
	if (queue->type == PARAM_QUEUE_TYPE_SET) {
		param_serialize_to_mpack(param, &writer, value);
	} else {
		mpack_write_u16(&writer, param_get_short_id(param, 0, 0));
	}
	queue->used = writer.used;
	return 0;
}

int param_queue_foreach(param_queue_t *queue, param_queue_callback_f callback) {

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, queue->buffer, queue->used);
	while(reader.left > 0) {
	    uint16_t short_id = mpack_expect_u16(&reader);
	    if (mpack_reader_error(&reader) != mpack_ok)
	    	return mpack_reader_error(&reader);

	    param_t * param = param_list_find_id(param_parse_short_id_node(short_id), param_parse_short_id_paramid(short_id));
	    if (param)
	    	callback(queue, param, &reader);

	}

	return mpack_ok;

}

int param_queue_print_callback(param_queue_t *queue, param_t *param, mpack_reader_t *reader) {
	printf("  %s:%u\t", param->name, param->node);
	if (queue->type == PARAM_QUEUE_TYPE_SET) {
		printf(" => ");
		mpack_print_element(reader, 2, stdout);
	}
	printf("\n");
	return 0;
}

void param_queue_print(param_queue_t *queue) {
	param_queue_foreach(queue, param_queue_print_callback);
}
