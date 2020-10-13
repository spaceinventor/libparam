/*
 * param_queue.c
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <mpack/mpack.h>
#include <mpack/mpack-config.h>

#include <param/param.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <param/param_list.h>

#include "param_serializer.h"
#include "param_string.h"

void param_queue_init(param_queue_t *queue, void *buffer, int buffer_size, int used, param_queue_type_e type, int version) {
	queue->buffer = buffer;
	queue->buffer_size = buffer_size;
	queue->type = type;
	queue->used = used;
	queue->version = version;
	queue->last_node = UINT16_MAX;
}

int param_queue_add(param_queue_t *queue, param_t *param, int offset, void *value) {
	mpack_writer_t writer;
	mpack_writer_init(&writer, queue->buffer, queue->buffer_size);
	writer.used = queue->used;
	if (queue->type == PARAM_QUEUE_TYPE_SET) {
		param_serialize_to_mpack(param, offset, &writer, value, queue);
	} else {
		param_serialize_id(&writer, param, offset, queue);
	}
	if (mpack_writer_error(&writer) != mpack_ok) {
		return -1;
	}
	queue->used = writer.used;
	return 0;
}

int param_queue_apply(param_queue_t *queue) {
	int return_code = 0;
	int atomic_write = 0;
	PARAM_QUEUE_FOREACH(param, reader, queue, offset)
		if (param) {
			if (param->mask & PM_ATOMIC_WRITE) {
				atomic_write = 1;
				printf("Atomic write begin %s[%d]\n", param->name, offset);

			}
			printf("Apply %s[%d]\n", param->name, offset);
			param_deserialize_from_mpack_to_param(NULL, queue, param, offset, &reader);
		} else {
			return_code = -1;
		}
	}

	if (atomic_write) {
		printf("Atomic write end\n");
	}

	return return_code;
}

void param_queue_print(param_queue_t *queue) {
	PARAM_QUEUE_FOREACH(param, reader, queue, offset)
		printf	("  %s:%u", param->name, param->node);
		if (offset >= 0) {
			printf("[%u]", offset);
		}
#if MPACK_STDIO
		printf(" = ");
		mpack_print_element((mpack_reader_t *) reader, 2, stdout);
#endif
		printf("\n");
	}
}

