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
	writer.position = queue->buffer + queue->used;
	if (queue->type == PARAM_QUEUE_TYPE_SET) {
		param_serialize_to_mpack(param, offset, &writer, value, queue);
	} else {
		param_serialize_id(&writer, param, offset, queue);
	}
	if (mpack_writer_error(&writer) != mpack_ok) {
		return -1;
	}
	queue->used = mpack_writer_buffer_used(&writer);
	return 0;
}

int param_queue_apply(param_queue_t *queue) {
	int return_code = 0;
	int atomic_write = 0;
	PARAM_QUEUE_FOREACH(param, reader, queue, offset)
		if (param) {
			if ((param->mask & PM_ATOMIC_WRITE) && (atomic_write == 0)) {
				atomic_write = 1;
				if (param_enter_critical)
					param_enter_critical();
			}
			param_deserialize_from_mpack_to_param(NULL, queue, param, offset, &reader);
		} else {
			// We couldn't find all parameters. Skip this one.
			return_code = -1;

			mpack_tag_t tag = mpack_read_tag(&reader);
			if (mpack_reader_error(&reader) != mpack_ok) {
				break;
			}

			// TODO: Skip content
			bool valid = true;

			switch (tag.type) {
    		case mpack_type_str:
    		case mpack_type_bin:
    			if (reader.end - reader.data >= tag.v.l) {
	    			mpack_skip_bytes(&reader, tag.v.l);
	    		} else {
    				valid = false;
	    			break;
	    		}

	    		if (tag.type == mpack_type_str) {
	    			mpack_done_str(&reader);
	    		} else if (tag.type == mpack_type_bin) {
	    			mpack_done_bin(&reader);
	    		}
    			break;
    		case mpack_type_array:
    			for (int i = 0; i < tag.v.n; i++) {
					mpack_read_tag(&reader);
					if (mpack_reader_error(&reader) != mpack_ok) {
						valid = false;
						break;
					}
    			}
    			if (valid) {
	    			mpack_done_array(&reader);
    			}
    			break;
    		case mpack_type_map:
    			break;
    		default:
    			break;
			}

		}
	}

	if (atomic_write) {
		if (param_exit_critical)
			param_exit_critical();
	}

	return return_code;
}

void param_queue_print(param_queue_t *queue) {
	PARAM_QUEUE_FOREACH(param, reader, queue, offset)
		if (param) {
			printf	("  %s:%u", param->name, param->node);
			if (offset >= 0) {
				printf("[%u]", offset);
			}
#if MPACK_STDIO
			printf(" = ");

			char buffer[1024];
    		mpack_print_t print;
    		mpack_memset(&print, 0, sizeof(print));
    		print.buffer = buffer;
    		print.size = sizeof(buffer);
			mpack_print_element(&reader, &print, 2);
			printf("%s", buffer);
#else
			mpack_discard(&reader);
#endif
			printf("\n");
		}
	}
}

