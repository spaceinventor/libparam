/*
 * param_queue.c
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#include <stdio.h>
#include <stdarg.h>

#include <csp/csp.h>
#include <csp/csp_hooks.h>
#include <mpack/mpack.h>

#include <param/param.h>
#include <param/param_server.h>
#include <param/param_queue.h>
#include <param/param_list.h>
#include <param/param_string.h>

#include <param/param_serializer.h>

#define PARAM_QUEUE_FOREACH(param, reader, queue, offset) \
	mpack_reader_t reader; \
	mpack_reader_init_data(&reader, queue->buffer, queue->used); \
	while(reader.data < reader.end) { \
		int id, node, offset = -1; \
		csp_timestamp_t timestamp = { .tv_sec = 0, .tv_nsec = 0 }; \
		param_deserialize_id(&reader, &id, &node, &timestamp, &offset, queue); \
		param_t * param = param_list_find_id(node, id); \

void param_queue_init(param_queue_t *queue, void *buffer, int buffer_size, int used, param_queue_type_e type, int version) {
	queue->buffer = buffer;
	queue->buffer_size = buffer_size;
	queue->type = type;
	queue->used = used;
	queue->version = version;
	queue->last_timestamp.tv_sec = 0;
	queue->last_timestamp.tv_nsec = 0;
	queue->client_timestamp.tv_sec = 0;
	queue->client_timestamp.tv_nsec = 0;
}

int param_queue_add(param_queue_t *queue, param_t *param, int offset, void *value) {

	/* Ensure we always send nodeid on the first element of the queue */
	if (queue->used == 0) {
		queue->last_node = UINT16_MAX;
	}

	if ((queue->type == PARAM_QUEUE_TYPE_GET) && (value != NULL)) {
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

int param_queue_apply_timestamp(param_queue_t *queue, int host, int verbose, const csp_timestamp_t *timestamp) {

	int return_code = 0;
	int atomic_write = 0;
	csp_timestamp_t time;

	if (timestamp == NULL) {
		csp_clock_get_time(&time);
	} else {
		time = *timestamp;
	}

	queue->last_timestamp = time;
	queue->client_timestamp = time;

	mpack_reader_t reader;
	mpack_reader_init_data(&reader, queue->buffer, queue->used);
	while(reader.data < reader.end) {
		int id, node, offset = -1;
		csp_timestamp_t timestamp = { .tv_sec = 0, .tv_nsec = 0 };
		param_deserialize_id(&reader, &id, &node, &timestamp, &offset, queue);

		/* For handling of responess on client side, replace localhost with host node */
		if (node == 0)
			node = host;

		/* Search on the specified node in the request or response */
		param_t * param = param_list_find_id(node, id);

		if (param) {
			if ((param->mask & PM_ATOMIC_WRITE) && (atomic_write == 0)) {
				atomic_write = 1;
				if (param_enter_critical)
					param_enter_critical();
			}

#ifdef PARAM_HAVE_TIMESTAMP
			/* Only remote paramters use timestamps */
			if (*param->node != 0) {
				/* If no timestamp was provided, use client received timestamp */
				if (timestamp.tv_sec == 0) {
					timestamp = queue->client_timestamp;
					csp_clock_get_time(&timestamp);
				} 
				*param->timestamp = timestamp;
			}
#endif

			param_deserialize_from_mpack_to_param(NULL, queue, param, offset, &reader);
		} else {
			// We couldn't find all parameters. Skip this one.
			return_code = -1;

			mpack_tag_t tag = mpack_read_tag(&reader);
			if (mpack_reader_error(&reader) != mpack_ok) {
				if (verbose >= 2) {
					printf("Param decoding failed for ID %u:%u, skipping packet\n", node, id);
				}
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

			if (verbose >= 3) {
				printf("Param ID %u:%u not found locally, skipping value\n", node, id);
			}
		}
	}

	if (atomic_write) {
		if (param_exit_critical)
			param_exit_critical();
	}

	return return_code;
}

int param_queue_apply(param_queue_t *queue, int host, int verbose) {

	return param_queue_apply_timestamp(queue, host, verbose, NULL);
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
			if (*param->node > 0) {
				printf("-n %d ", *param->node);
			}
			printf("%s", param->name);
			if (offset >= 0) {
				printf("[%u] ", offset);
			} else {
				printf(" ");
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
		} else {
			printf("param definition for ID: %d on node: %d not loaded, run \"list add\" maybe ?\n", id, node);
		}
	}
}

void param_queue_print_params(param_queue_t *queue, uint32_t ref_timestamp) {
	int outer_count = 0;
	PARAM_QUEUE_FOREACH(param, reader, queue, offset)
		int already_printed = 0;
		int inner_counter = 0;
		if(param){
			mpack_reader_t _reader;
			mpack_reader_init_data(&_reader, queue->buffer, queue->used);
			while(outer_count > inner_counter) {
				int _id, _node, _offset = -1;
				csp_timestamp_t _timestamp = { .tv_sec = 0, .tv_nsec = 0 };
				param_deserialize_id(&_reader, &_id, &_node, &_timestamp, &_offset, queue);
				param_t * _param = param_list_find_id(_node, _id);
				if(queue->type == PARAM_QUEUE_TYPE_SET){
					mpack_discard(&_reader);
				}
				if(_param == param){
					already_printed = 1;
					break;
				}
				inner_counter++;
			}
			if(!already_printed){
				param_print(param, -1, NULL, 0, 2, ref_timestamp);
			}
		}
		if(queue->type == PARAM_QUEUE_TYPE_SET){
			mpack_discard(&reader);
		}
		outer_count++;
	}
}
