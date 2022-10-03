/*
 * param_serializer.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <param/param.h>
#include <param/param_server.h>
#include "param_serializer.h"

#include <csp/arch/csp_time.h>
#include <sys/types.h>
#include <csp/csp.h>
#include <param/param_list.h>

#include <mpack/mpack.h>

static inline uint16_t param_get_short_id(param_t * param, unsigned int isarray, unsigned int reserved) {
	uint16_t node = param->node;
	return (node << 11) | ((isarray & 0x1) << 10) | ((reserved & 0x1) << 2) | ((param->id) & 0x1FF);
}

static inline uint8_t param_parse_short_id_flag_isarray(uint16_t short_id) {
	return (short_id >> 10) & 0x1;
}

static inline uint8_t param_parse_short_id_node(uint16_t short_id) {
	return (short_id >> 11) & 0x1F;
}

static inline uint16_t param_parse_short_id_paramid(uint16_t short_id) {
	return short_id & 0x1FF;
}

void param_serialize_id(mpack_writer_t *writer, param_t *param, int offset, param_queue_t *queue) {

	if (queue->version == 1) {

		if (offset >= 0) {
			mpack_write_u16(writer, param_get_short_id(param, 1, 0));
			char _offset = offset;
			mpack_write_bytes(writer, &_offset, 1);
		} else {
			mpack_write_u16(writer, param_get_short_id(param, 0, 0));
		}

	} else {

		int node = param->node;
		uint32_t timestamp = param->timestamp;
		int array_flag = (offset >= 0) ? 1 : 0;
		int node_flag = (queue->last_node != node) ? 1 : 0;
		int timestamp_flag = (queue->last_timestamp != timestamp) ? 1 : 0;
		int extendedid_flag = (param->id > 0x3ff) ? 1 : 0;

		uint16_t header = array_flag << 15 | node_flag << 14 | timestamp_flag << 13 | extendedid_flag << 12 | (param->id & 0x3ff);
		header = htobe16(header);
		mpack_write_u16(writer, header);

		if (array_flag) {
			char _offset = offset;
			mpack_write_bytes(writer, &_offset, 1);
		}

		if (node_flag) {
			queue->last_node = node;
			uint16_t _node = htobe16(node);
			mpack_write_bytes(writer, (char*) &_node, 2);
		}

		if (timestamp_flag) {
			queue->last_timestamp = timestamp;
			uint32_t _timestamp = htobe32(timestamp);
			mpack_write_bytes(writer, (char*) &_timestamp, 4);
		}

		if (extendedid_flag) {
			char _extendedid = (param->id&0xfc00) >> 8;
			mpack_write_bytes(writer, &_extendedid, 1);
		}

	}

}

void param_deserialize_id(mpack_reader_t *reader, int *id, int *node, long unsigned int *timestamp, int *offset, param_queue_t *queue) {

	if (queue->version == 1) {

		uint16_t short_id = mpack_expect_u16(reader);

		if (mpack_reader_error(reader) != mpack_ok)
			return;

		if (param_parse_short_id_flag_isarray(short_id)) {
			char _offset;
			mpack_read_bytes(reader, &_offset, 1);
			*offset = _offset;
		}

		*id = param_parse_short_id_paramid(short_id);
		*node = param_parse_short_id_node(short_id);

	} else {

		uint16_t header = mpack_expect_u16(reader);
		if (mpack_reader_error(reader) != mpack_ok)
			return;

		header = be16toh(header);
		int array_flag = header & 0x8000;
		int node_flag = header & 0x4000;
		int timestamp_flag = header & 0x2000;
		int extendedid_flag = header & 0x1000;
		*id = header & 0x3ff;

		if (array_flag) {
			char _offset;
			mpack_read_bytes(reader, &_offset, 1);
			*offset = _offset;
		}

		if (node_flag) {
			uint16_t _node;
			mpack_read_bytes(reader, (char*) &_node, 2);
			_node = be16toh(_node);
			*node = _node;
			queue->last_node = _node;
		} else {
			*node = queue->last_node;
		}


		if (timestamp_flag) {
			uint32_t _timestamp;
			mpack_read_bytes(reader, (char*) &_timestamp, 4);
			_timestamp = be32toh(_timestamp);
			*timestamp = _timestamp;
			queue->last_timestamp = _timestamp;
		} else {
			*timestamp = queue->last_timestamp;
		}

		if (extendedid_flag) {
			char _extendedid;
			mpack_read_bytes(reader, &_extendedid, 1);
			*id |= (uint16_t)_extendedid << 8;
		}

	}

}

int param_serialize_to_mpack(param_t * param, int offset, mpack_writer_t * writer, void * value, param_queue_t * queue) {

	/* Remember the initial position if we need to abort later due to buffer full */
	char * init_pos = writer->position;

	param_serialize_id(writer, param, offset, queue);

	if (mpack_writer_error(writer) != mpack_ok)
		return -1;


	int count = (param->array_size > 0) ? param->array_size : 1;

	/* Treat data and strings as single parameters */
	if (param->type == PARAM_TYPE_DATA || param->type == PARAM_TYPE_STRING)
		count = 1;

	/* If offset is set, adjust count to only display one value */
	if (offset >= 0) {
		count = 1;
	}

	/* If offset is unset, start at zero and display all values */
	if (offset < 0) {
		offset = 0;
	}

	if (count > 1) {
		mpack_start_array(writer, count);
	}

	for(int i = offset; i < offset + count; i++) {

		switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			if (value) {
				mpack_write_uint(writer, *(uint8_t *) value);
			} else {
				mpack_write_uint(writer, param_get_uint8_array(param, i));
			}
			break;
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			if (value) {
				mpack_write_uint(writer, *(uint16_t *) value);
			} else {
				mpack_write_uint(writer, param_get_uint16_array(param, i));
			}
			break;
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			if (value) {
				mpack_write_uint(writer, *(uint32_t *) value);
			} else {
				mpack_write_uint(writer, param_get_uint32_array(param, i));
			}
			break;
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			if (value) {
				mpack_write_uint(writer, *(uint64_t *) value);
			} else {
				mpack_write_uint(writer, param_get_uint64_array(param, i));
			}
			break;
		case PARAM_TYPE_INT8:
			if (value) {
				mpack_write_int(writer, *(int8_t *) value);
			} else {
				mpack_write_int(writer, param_get_int8_array(param, i));
			}
			break;
		case PARAM_TYPE_INT16:
			if (value) {
				mpack_write_int(writer, *(int16_t *) value);
			} else {
				mpack_write_int(writer, param_get_int16_array(param, i));
			}
			break;
		case PARAM_TYPE_INT32:
			if (value) {
				mpack_write_int(writer, *(int32_t *) value);
			} else {
				mpack_write_int(writer, param_get_int32_array(param, i));
			}
			break;
		case PARAM_TYPE_INT64:
			if (value) {
				mpack_write_int(writer, *(int64_t *) value);
			} else {
				mpack_write_int(writer, param_get_int64_array(param, i));
			}
			break;
#if MPACK_FLOAT
		case PARAM_TYPE_FLOAT:
			if (value) {
				mpack_write_float(writer, *(float *) value);
			} else {
				mpack_write_float(writer, param_get_float_array(param, i));
			}
			break;
		case PARAM_TYPE_DOUBLE:
			if (value) {
				mpack_write_double(writer, *(double *) value);
			} else {
				mpack_write_double(writer, param_get_double_array(param, i));
			}
			break;
#endif
		case PARAM_TYPE_STRING: {
			size_t len;
			if (value) {
				len = strnlen(value, param->array_size);

				mpack_start_str(writer, len);

				if (writer->position + len > writer->end) {
					writer->error = mpack_error_too_big;
					break;
				}

				memcpy(writer->position, (char *) value, len);

			} else {
				char tmp[param->array_size];
				param_get_data(param, tmp, param->array_size);
				len = strnlen(tmp, param->array_size);

				mpack_start_str(writer, len);

				if (writer->position + len > writer->end) {
					writer->error = mpack_error_too_big;
					break;
				}

				memcpy(writer->position, tmp, len);

			}
			writer->position += len;
			mpack_finish_str(writer);
			break;
		}

		case PARAM_TYPE_DATA:

			mpack_start_bin(writer, param->array_size);

			unsigned int size = (param->array_size > 0) ? param->array_size : 1;
			if (writer->position + size > writer->end) {
				writer->error = mpack_error_too_big;
				break;
			}

			if (value) {
				memcpy(writer->position, value, size);
			} else {
				param_get_data(param, writer->position, size);
			}
			writer->position += param->array_size;
			mpack_finish_bin(writer);
			break;

		default:
			break;
		}

		if (mpack_writer_error(writer) != mpack_ok) {
			writer->position = init_pos;
			return -1;
		}

	}

	if (count > 1) {
		mpack_finish_array(writer);
	}

	return 0;

}

void param_deserialize_from_mpack_to_param(void * context, void * queue, param_t * param, int offset, mpack_reader_t * reader) {

	if (offset < 0)
		offset = 0;

	int count = 1;

	/* Inspect for array */
	mpack_tag_t tag = mpack_peek_tag(reader);
	if (tag.type == mpack_type_array) {
		count = mpack_expect_array(reader);
	}

	for (int i = offset; i < offset + count; i++) {

		switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			param_set_uint8_array(param, i, (uint8_t) mpack_expect_uint(reader)); break;
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			param_set_uint16_array(param, i, (uint16_t) mpack_expect_uint(reader)); break;
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			param_set_uint32_array(param, i, (uint32_t) mpack_expect_uint(reader)); break;
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			param_set_uint64_array(param, i, mpack_expect_u64(reader)); break;
		case PARAM_TYPE_INT8:
			param_set_int8_array(param, i, (int8_t) mpack_expect_int(reader)); break;
		case PARAM_TYPE_INT16:
			param_set_int16_array(param, i, (int16_t) mpack_expect_int(reader)); break;
		case PARAM_TYPE_INT32:
			param_set_int32_array(param, i, (int32_t) mpack_expect_int(reader)); break;
		case PARAM_TYPE_INT64:
			param_set_int64_array(param, i, mpack_expect_i64(reader)); break;
#if MPACK_FLOAT
		case PARAM_TYPE_FLOAT:
			param_set_float_array(param, i, mpack_expect_float(reader)); break;
		case PARAM_TYPE_DOUBLE:
			param_set_double_array(param, i, mpack_expect_double(reader)); break;
#endif
		case PARAM_TYPE_STRING: {
			int len = mpack_expect_str(reader);
			if (len == 0) {
				param_set_string(param, "", 1);
			} else {
				param_set_string(param, (void *) reader->data, len);
			}
			reader->data += len;
			mpack_done_str(reader);
			break;
		}
		case PARAM_TYPE_DATA: {
			int len = mpack_expect_bin(reader);
			param_set_data(param, (void *) reader->data, len);
			reader->data += len;
			mpack_done_bin(reader);
			break;
		}

		default:
			mpack_discard(reader);
			break;
		}

		if (mpack_reader_error(reader) != mpack_ok) {
			return;
		}

	}

}
