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

param_queue_t * param_queue_create(void * buffer, int buffer_length, param_queue_type_e type) {

	param_queue_t * queue = malloc(sizeof(param_queue_t));

	if (buffer) {
		queue->extbuffer = buffer;
		mpack_writer_init(&queue->writer, queue->extbuffer, buffer_length);
	} else {
		queue->intbuffer = malloc(buffer_length);
		mpack_writer_init(&queue->writer, queue->intbuffer, buffer_length);
	}

	queue->type = type;
	return queue;

}

void param_queue_destroy(param_queue_t *queue) {
	mpack_writer_destroy(&queue->writer);
	if (queue->intbuffer)
		free(queue->intbuffer);
	free(queue);
}

void param_queue_print(param_queue_t *queue) {

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, queue->writer.buffer, queue->writer.used);

	size_t remaining;
	while((remaining = mpack_reader_remaining(&reader, NULL)) > 0) {
	    uint16_t short_id = mpack_expect_u16(&reader);

	    param_t * param = param_list_find_id(param_parse_short_id_node(short_id), param_parse_short_id_paramid(short_id));
	    if (param) {
	    	printf("  %s:%u\t", param->name, param->node);
	    } else {
	    	printf("  %u:%u\t", param_parse_short_id_node(short_id), param_parse_short_id_paramid(short_id));
	    }
	    if (queue->type == PARAM_QUEUE_TYPE_SET) {
	    	printf(" => ");
	    	mpack_print_element(&reader, 2, stdout);
	    }

	    printf("\n");
	    if (mpack_reader_error(&reader) != mpack_ok)
	    	break;
	}

    if (mpack_reader_destroy(&reader) != mpack_ok) {
        printf("<mpack parsing error %s>\n", mpack_error_to_string(mpack_reader_error(&reader)));
	}

}

int param_queue_push(param_queue_t *queue, param_t *param, void *value) {
	if (queue->type == PARAM_QUEUE_TYPE_SET) {
		param_serialize_to_mpack(param, &queue->writer, value);
	} else {
		mpack_write_u16(&queue->writer, param_get_short_id(param, 0, 0));
	}
	return 0;
}
