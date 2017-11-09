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
		int len = strlen(value);
		if (len > param->size)
			len = param->size;
		mpack_start_str(writer, len);
		if (value) {
			memcpy(writer->buffer + writer->used, value, len);
		} else {
			param_get_data(param, writer->buffer + writer->used, len);
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

void param_deserialize_from_mpack_map(mpack_reader_t * reader) {

	// TODO: Implement arrays as value
	unsigned int i = 0;

	uint16_t short_id = mpack_expect_u16(reader);
	param_t * param = param_list_find_id(param_parse_short_id_node(short_id),
			param_parse_short_id_paramid(short_id));
	if (param == NULL)
		return;

	if (param->storage_type != PARAM_STORAGE_REMOTE) {
		printf("Error %s is not remote\n", param->name);
		mpack_discard(reader);
		return;
	}

	param->value_pending = 0;

	switch (param->type) {
	case PARAM_TYPE_UINT8:
		*(uint8_t*) (param->value_get + i * sizeof(uint8_t)) =
				(uint8_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_UINT16:
		*(uint16_t*) (param->value_get + i * sizeof(uint16_t)) =
				(uint16_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_UINT32:
		*(uint32_t*) (param->value_get + i * sizeof(uint32_t)) =
				(uint32_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_UINT64:
		*(uint64_t*) (param->value_get + i * sizeof(uint64_t)) =
				(uint64_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_XINT8:
		*(uint8_t*) (param->value_get + i * sizeof(uint8_t)) =
				(uint8_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_XINT16:
		*(uint16_t*) (param->value_get + i * sizeof(uint16_t)) =
				(uint16_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_XINT32:
		*(uint32_t*) (param->value_get + i * sizeof(uint32_t)) =
				(uint32_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_XINT64:
		*(uint64_t*) (param->value_get + i * sizeof(uint64_t)) =
				(uint64_t) mpack_expect_uint(reader);
		break;
	case PARAM_TYPE_INT8:
		*(int8_t*) (param->value_get + i * sizeof(int8_t)) =
				(int8_t) mpack_expect_int(reader);
		break;
	case PARAM_TYPE_INT16:
		*(int16_t*) (param->value_get + i * sizeof(int16_t)) =
				(int16_t) mpack_expect_int(reader);
		break;
	case PARAM_TYPE_INT32:
		*(int32_t*) (param->value_get + i * sizeof(int32_t)) =
				(int32_t) mpack_expect_int(reader);
		break;
	case PARAM_TYPE_INT64:
		*(int64_t*) (param->value_get + i * sizeof(int64_t)) =
				(int64_t) mpack_expect_int(reader);
		break;
	case PARAM_TYPE_FLOAT:
		*(float*) (param->value_get + i * sizeof(float)) =
				(float) mpack_expect_float(reader);
		break;
	case PARAM_TYPE_DOUBLE:
		*(double*) (param->value_get + i * sizeof(double)) =
				(double) mpack_expect_double(reader);
		break;
	case PARAM_TYPE_STRING:
		mpack_expect_str_buf(reader, param->value_get, param->size);
		break;
	case PARAM_TYPE_DATA:
		mpack_expect_bin_buf(reader, param->value_get, param->size);
		break;

	default:
	case PARAM_TYPE_VECTOR3:
		mpack_discard(reader);
		break;
	}

}
