/*
 * param_queue.c
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <mpack/mpack.h>

#include <param/param.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <param/param_list.h>
#include <param/param_string.h>

#include "param_serializer.h"

void param_queue_init(param_queue_t *queue, void *buffer, int buffer_size, int used, param_queue_type_e type, int version) {
	queue->buffer = buffer;
	queue->buffer_size = buffer_size;
	queue->type = type;
	queue->used = used;
	queue->version = version;
	queue->last_timestamp = 0;
}

int param_queue_add(param_queue_t *queue, param_t *param, int offset, void *value) {

	/* Ensure we always send nodeid on the first element of the queue */
	if (queue->used == 0) {
		queue->last_node = UINT16_MAX;
	}

	if ((queue->type == PARAM_QUEUE_TYPE_GET) && (value != NULL)) {
		printf("Cannot mix GET/SET commands\n");
		printf("Queue type %u value %p\n", queue->type, value);
		return -1;
	}

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

int param_queue_apply(param_queue_t *queue, int apply_local, int from, int store_timestamp) {
	int return_code = 0;
	int atomic_write = 0;

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, queue->buffer, queue->used);
	while(reader.data < reader.end) {
		int id, node, offset = -1;
		long unsigned int timestamp = 0;
		param_deserialize_id(&reader, &id, &node, &timestamp, &offset, queue);

		/* If the from address is set, and the nodeid is 0, substitue with the source address */
		if (node == 0)
			node = from;

		/* First we search on the specified node in the request or response */
		param_t * param = param_list_find_id(node, id);

		if (!param) {

			/* If the apply_local flag is set (true for push messages) and the parameter not found,
			   try a search in the local parameters: This enables the re-use of packets from one node
			   to another. But be very carefull with this. */
			if (apply_local == 1) {
				node = 0;
			}

			param = param_list_find_id(node, id);
		}

		if (param) {
			if ((param->mask & PM_ATOMIC_WRITE) && (atomic_write == 0)) {
				atomic_write = 1;
				if (param_enter_critical)
					param_enter_critical();
			}

			if (store_timestamp)
				param->timestamp = timestamp;

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
	if (queue->type == PARAM_QUEUE_TYPE_GET) {
		printf("cmd new get %s\n", queue->name);
	} else if (queue->type == PARAM_QUEUE_TYPE_SET) {
		printf("cmd new set %s\n", queue->name);
	}
	PARAM_QUEUE_FOREACH(param, reader, queue, offset)
		if (param) {
			printf("cmd add ");
			if (param->node > 0) {
				printf("-n %d ", param->node);
			}
			printf("%s ", param->name);
			if (offset >= 0) {
				printf("[%u]", offset);
			}
			if (queue->type == PARAM_QUEUE_TYPE_SET) {
#if MPACK_STDIO

				char buffer[20] = {0};
				mpack_print_t print;
				mpack_memset(&print, 0, sizeof(print));
				print.buffer = buffer;
				print.size = sizeof(buffer);
				mpack_print_element(&reader, &print, 2);
				printf("%s", buffer);
#else
				mpack_discard(&reader);
#endif
			}
			printf("\n");
		}
	}
}

