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

param_queue_t * param_queue_create(void) {

	csp_packet_t * buffer = csp_buffer_get(256);
	if (!buffer)
		return NULL;

	param_queue_t * queue = malloc(sizeof(param_queue_t));
	queue->buffer = buffer;

	mpack_writer_init(&queue->writer, (char *) queue->buffer, 256);

	return queue;
}

void param_queue_destroy(param_queue_t *queue) {
	mpack_writer_destroy(&queue->writer);
	if (queue->buffer)
		csp_buffer_free(queue->buffer);
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
