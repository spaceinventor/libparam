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
#include "param_serializer.h"

param_queue_t * param_queue_create(void * buffer, int buffer_length) {

	param_queue_t * queue = malloc(sizeof(param_queue_t));

	if (buffer) {
		queue->buffer = buffer;
	} else {
		queue->buffer = malloc(buffer_length);
	}

	mpack_writer_init(&queue->writer, queue->buffer, buffer_length);
	printf("writer buffer %p csp buffer %p\n", queue->writer.buffer, queue->buffer);

	return queue;
}

void param_queue_destroy(param_queue_t *queue) {
	mpack_writer_destroy(&queue->writer);
	if (queue->buffer)
		free(queue->buffer);
	free(queue);
}

void param_queue_print(param_queue_t *queue) {

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, queue->writer.buffer, queue->writer.used);

	size_t remaining;
	while((remaining = mpack_reader_remaining(&reader, NULL)) > 0) {
	    uint16_t short_id = mpack_expect_u16(&reader);
	    printf("node %u, id: %u = ", param_parse_short_id_node(short_id), param_parse_short_id_paramid(short_id));
	    mpack_print_element(&reader, 2, stdout);
	    printf("\n");
	    if (mpack_reader_error(&reader) != mpack_ok)
	    	break;
	}

    if (mpack_reader_destroy(&reader) != mpack_ok) {
        printf("<mpack parsing error %s>\n", mpack_error_to_string(mpack_reader_error(&reader)));
	}

}

int param_queue_add(param_queue_t *queue, param_t *param, void *value) {
	param_serialize_to_mpack(param, &queue->writer, value);
	return 0;
}
