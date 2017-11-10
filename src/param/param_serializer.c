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
#include "param_log.h"
#include "param_serializer.h"
#include "param_string.h"

#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>
#include <csp/csp.h>
#include <param/param_list.h>

#include <mpack/mpack.h>

void param_serialize_to_mpack(param_t * param, mpack_writer_t * writer, void * value) {
	mpack_write_u16(writer, param_get_short_id(param, 0, 0));

	// TODO: Implement arrays

	//printf("param %s value %p\n", param->name, value);

	switch (param->type) {
	case PARAM_TYPE_UINT8:
	case PARAM_TYPE_XINT8:
		if (value) {
			mpack_write_uint(writer, *(uint8_t *) value);
		} else {
			mpack_write_uint(writer, param_get_uint8(param));
		}
		break;
	case PARAM_TYPE_UINT16:
	case PARAM_TYPE_XINT16:
		if (value) {
			mpack_write_uint(writer, *(uint16_t *) value);
		} else {
			mpack_write_uint(writer, param_get_uint16(param));
		}
		break;
	case PARAM_TYPE_UINT32:
	case PARAM_TYPE_XINT32:
		if (value) {
			printf("Set u32 %u\n", *(uint32_t *) value);
			mpack_write_uint(writer, *(uint32_t *) value);
		} else {
			mpack_write_uint(writer, param_get_uint32(param));
		}
		break;
	case PARAM_TYPE_UINT64:
	case PARAM_TYPE_XINT64:
		if (value) {
			mpack_write_uint(writer, *(uint64_t *) value);
		} else {
			mpack_write_uint(writer, param_get_uint64(param));
		}
		break;
	case PARAM_TYPE_INT8:
		if (value) {
			mpack_write_int(writer, *(int8_t *) value);
		} else {
			mpack_write_int(writer, param_get_int8(param));
		}
		break;
	case PARAM_TYPE_INT16:
		if (value) {
			mpack_write_int(writer, *(int16_t *) value);
		} else {
			mpack_write_int(writer, param_get_int16(param));
		}
		break;
	case PARAM_TYPE_INT32:
		if (value) {
			mpack_write_int(writer, *(int32_t *) value);
		} else {
			mpack_write_int(writer, param_get_int32(param));
		}
		break;
	case PARAM_TYPE_INT64:
		if (value) {
			mpack_write_int(writer, *(int64_t *) value);
		} else {
			mpack_write_int(writer, param_get_int64(param));
		}
		break;
	case PARAM_TYPE_FLOAT:
		if (value) {
			mpack_write_float(writer, *(float *) value);
		} else {
			mpack_write_float(writer, param_get_float(param));
		}
		break;
	case PARAM_TYPE_DOUBLE:
		if (value) {
			mpack_write_double(writer, *(double *) value);
		} else {
			mpack_write_double(writer, param_get_double(param));
		}
		break;

	case PARAM_TYPE_STRING: {
		int len;
		if (value) {
			len = strnlen(value, param->size);
			mpack_start_str(writer, len);
			memcpy(writer->buffer + writer->used, (char *) value, len);
		} else {
			char tmp[param->size];
			param_get_data(param, tmp, param->size);
			len = strnlen(tmp, param->size);
			mpack_start_str(writer, len);
			memcpy(writer->buffer + writer->used, tmp, len);
		}
		writer->used += len;
		mpack_finish_str(writer);
		break;
	}

	case PARAM_TYPE_DATA:
		mpack_start_bin(writer, param->size);
		if (value) {
			memcpy(writer->buffer + writer->used, value, param->size);
		} else {
			param_get_data(param, writer->buffer + writer->used, param->size);
		}
		writer->used += param->size;
		mpack_finish_bin(writer);
		break;

	default:
	case PARAM_TYPE_VECTOR3:
		break;
	}

}

param_t * param_deserialize_from_mpack(mpack_reader_t * reader) {

	// TODO: Implement arrays as value
	unsigned int i = 0;

	uint16_t short_id = mpack_expect_u16(reader);
	param_t * param = param_list_find_id(param_parse_short_id_node(short_id),
			param_parse_short_id_paramid(short_id));
	if (param == NULL)
		return NULL;

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
	case PARAM_TYPE_FLOAT:
		param_set_float_array(param, i, mpack_expect_float(reader)); break;
	case PARAM_TYPE_DOUBLE:
		param_set_double_array(param, i, mpack_expect_double(reader)); break;
	case PARAM_TYPE_STRING: {
		int len = mpack_expect_str(reader);
		param_set_string(param, &reader->buffer[reader->pos], len);
		reader->pos += len;
		reader->left -= len;
		mpack_done_str(reader);
		break;
	}
	case PARAM_TYPE_DATA: {
		int len = mpack_expect_bin(reader);
		param_set_data(param, &reader->buffer[reader->pos], len);
		reader->pos += len;
		reader->left -= len;
		mpack_done_bin(reader);
		break;
	}

	default:
	case PARAM_TYPE_VECTOR3:
		mpack_discard(reader);
		break;
	}

	return param;

}
