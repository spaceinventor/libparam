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

int param_deserialize_to_var(param_type_e type, int size, void * in, void * out)
{

	int count = 0;

	switch(type) {

#define PARAM_DESERIALIZE(_case, _type, name, swapfct) \
		case _case: { \
			_type * objptr = (_type *) out; \
			memcpy(objptr, in, sizeof(_type)); \
			count += sizeof(_type); \
			*objptr = swapfct(*objptr); \
			break; \
		}

		PARAM_DESERIALIZE(PARAM_TYPE_UINT8, uint8_t, uint8, )
		PARAM_DESERIALIZE(PARAM_TYPE_UINT16, uint16_t, uint16, csp_ntoh16)
		PARAM_DESERIALIZE(PARAM_TYPE_UINT32, uint32_t, uint32, csp_ntoh32)
		PARAM_DESERIALIZE(PARAM_TYPE_UINT64, uint64_t, uint64, csp_ntoh64)
		PARAM_DESERIALIZE(PARAM_TYPE_INT8, int8_t, int8, )
		PARAM_DESERIALIZE(PARAM_TYPE_INT16, int16_t, int16, csp_ntoh16)
		PARAM_DESERIALIZE(PARAM_TYPE_INT32, int32_t, int32, csp_ntoh32)
		PARAM_DESERIALIZE(PARAM_TYPE_INT64, int64_t, int64, csp_ntoh64)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_DESERIALIZE(PARAM_TYPE_XINT16, uint16_t, uint16, csp_ntoh16)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT32, uint32_t, uint32, csp_ntoh32)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT64, uint64_t, uint64, csp_ntoh64)
		PARAM_DESERIALIZE(PARAM_TYPE_FLOAT, float, float, )
		PARAM_DESERIALIZE(PARAM_TYPE_DOUBLE, double, double, )
		PARAM_DESERIALIZE(PARAM_TYPE_VECTOR3, param_type_vector3, param_type_vector3, )

#undef PARAM_DESERIALIZE

		case PARAM_TYPE_DATA:
		case PARAM_TYPE_STRING:
			memcpy(out, in, size);
			count += size;
			break;

	}

	return count;

}

int param_serialize_from_var(param_type_e type, int size, void * in, char * out)
{

	int count = 0;

	switch(type) {

#define PARAM_SERIALIZE(_case, _type, name, _swapfct) \
		case _case: { \
			_type obj = *(_type *) in; \
			obj = _swapfct(obj); \
			memcpy(out, &obj, sizeof(_type)); \
			count += sizeof(_type); \
			break; \
		}

		PARAM_SERIALIZE(PARAM_TYPE_UINT8, uint8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_UINT16, uint16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_UINT32, uint32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_UINT64, uint64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_INT8, int8_t, int8, )
		PARAM_SERIALIZE(PARAM_TYPE_INT16, int16_t, int16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_INT32, int32_t, int32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_INT64, int64_t, int64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_XINT16, uint16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_XINT32, uint32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_XINT64, uint64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_FLOAT, float, float, )
		PARAM_SERIALIZE(PARAM_TYPE_DOUBLE, double, double, )

		case PARAM_TYPE_VECTOR3:
		case PARAM_TYPE_STRING:
		case PARAM_TYPE_DATA:
			memcpy(out, in, size);
			count += size;
			break;

#undef PARAM_SERIALIZE
	}

	return count;

}

int param_serialize_chunk_param_and_value(param_t * params[], uint8_t count, uint8_t * out, int pending_only) {

	out[0] = PARAM_CHUNK_PARAM_AND_VALUE;
	out[1] = 0;

	int outset = 2;
	for (int i = 0; i < count; i++) {

		/* Filter:
		 * When sending pending parameters (push) we wish to skip non pending parameters */
		if (pending_only == 1) {
			if ((params[i]->value_set == NULL) || (params[i]->value_pending != 1))
				continue;
		}

		if (outset + sizeof(uint16_t) + param_size(params[i]) > PARAM_SERVER_MTU) {
			printf("Request cropped: > MTU\n");
			break;
		}

		/* ID */
		uint16_t param_net = csp_hton16(params[i]->id);
		memcpy(&out[outset], &param_net, sizeof(param_net));
		outset += sizeof(param_net);

		/* Get actual value */
		if (pending_only == 0) {
			char tmp[param_size(params[i])];
			param_get(params[i], 0, tmp);
			outset += param_serialize_from_var(params[i]->type, param_size(params[i]), tmp, (char *) &out[outset]);

		/* Get pending value */
		} else {
			outset += param_serialize_from_var(params[i]->type, param_size(params[i]), params[i]->value_set, (char *) &out[outset]);
		}

		/* Increment parameter count */
		out[1] += 1;

	}

	return outset;
}

int param_deserialize_chunk_param_and_value(uint8_t node, uint32_t timestamp, int verbose, uint8_t * in) {
	int count = in[1];

	int inset = 2;
	for (int i = 0; i < count; i++) {

		uint16_t id;
		memcpy(&id, &in[inset], sizeof(id));
		inset += sizeof(id);
		id = csp_ntoh16(id);

		param_t * param = param_list_find_id(node, id);
		if (param == NULL) {
			/** TODO: Possibly use segment length, instead of parameter count, making it possible to skip a segment */
			printf("Invalid param %u:%u\n", id, node);
			return 1000; // TODO proper error handling
		}

		if (param->storage_type == PARAM_STORAGE_REMOTE) {

			inset += param_deserialize_to_var(param->type, param->size, &in[inset], param->value_get);

			/**
			 * TODO: value updated is used by collector (refresh interval)
			 * So maybe it should not be used for logging, because the remote timestamp could be invalid
			 * The timestamp could be used for the param_log call instead.
			 */
			param->value_updated = timestamp;
			if (param->value_pending == 2)
				param->value_pending = 0;

		/* Local set */
		} else {
			char tmp[param_size(param)];
			inset += param_deserialize_to_var(param->type, param->size, &in[inset], tmp);
			param_set(param, 0, tmp);
		}

		if (verbose)
			param_print(param, -1, NULL, 0, 0);

		/**
		 * TODO: Param log assumes ordered input
		 */
		//param_log(param, param->value_get, timestamp);

	}
	return inset;
}

void param_serialize_to_mpack_map(param_t * param, mpack_writer_t * writer) {
	mpack_write_u16(writer, param_get_short_id(param, 0 , 0));

	switch(param->type) {
	case PARAM_TYPE_UINT8:		mpack_write_uint(writer, param_get_uint8(param)); break;
	case PARAM_TYPE_UINT16:		mpack_write_uint(writer, param_get_uint16(param)); break;
	case PARAM_TYPE_UINT32:		mpack_write_uint(writer, param_get_uint32(param)); break;
	case PARAM_TYPE_UINT64:		mpack_write_uint(writer, param_get_uint64(param)); break;
	case PARAM_TYPE_XINT8:		mpack_write_uint(writer, param_get_uint8(param)); break;
	case PARAM_TYPE_XINT16:		mpack_write_uint(writer, param_get_uint16(param)); break;
	case PARAM_TYPE_XINT32:		mpack_write_uint(writer, param_get_uint32(param)); break;
	case PARAM_TYPE_XINT64:		mpack_write_uint(writer, param_get_uint64(param)); break;
	case PARAM_TYPE_INT8:		mpack_write_int(writer, param_get_int8(param)); break;
	case PARAM_TYPE_INT16:		mpack_write_int(writer, param_get_int16(param)); break;
	case PARAM_TYPE_INT32:		mpack_write_int(writer, param_get_int32(param)); break;
	case PARAM_TYPE_INT64:		mpack_write_int(writer, param_get_int64(param)); break;
	case PARAM_TYPE_FLOAT:		mpack_write_float(writer, param_get_float(param)); break;
	case PARAM_TYPE_DOUBLE:		mpack_write_double(writer, param_get_double(param)); break;

	case PARAM_TYPE_STRING:
		mpack_start_str(writer, param->size);
		param_get_data(param, writer->buffer + writer->used, param->size);
		writer->used += param->size;
		mpack_finish_str(writer);
		break;

	case PARAM_TYPE_DATA:
		mpack_start_bin(writer, param->size);
		param_get_data(param, writer->buffer + writer->used, param->size);
		writer->used += param->size;
		mpack_finish_bin(writer);
		break;

	default:
	case PARAM_TYPE_VECTOR3:
		break;
	}

}

void param_deserialize_from_mpack_map(mpack_reader_t * reader) {
	uint16_t short_id = mpack_expect_u16(reader);
	param_t * param = param_list_find_id(param_parse_short_id_node(short_id), param_parse_short_id_paramid(short_id));
	if (param == NULL)
		return;

	printf("deserialize %s\n", param->name);

	switch(param->type) {
	case PARAM_TYPE_UINT8:		param_set_uint8(param, (uint8_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_UINT16:		param_set_uint16(param, (uint16_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_UINT32:		param_set_uint32(param, (uint32_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_UINT64:		param_set_uint64(param, (uint64_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_XINT8:		param_set_uint8(param, (uint8_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_XINT16:		param_set_uint16(param, (uint16_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_XINT32:		param_set_uint32(param, (uint32_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_XINT64:		param_set_uint64(param, (uint64_t) mpack_expect_uint(reader)); break;
	case PARAM_TYPE_INT8:		param_set_int8(param, (int8_t) mpack_expect_int(reader)); break;
	case PARAM_TYPE_INT16:		param_set_int16(param, (int16_t) mpack_expect_int(reader)); break;
	case PARAM_TYPE_INT32:		param_set_int32(param, (int32_t) mpack_expect_int(reader)); break;
	case PARAM_TYPE_INT64:		param_set_int64(param, (int64_t) mpack_expect_int(reader)); break;

	case PARAM_TYPE_FLOAT:
		param_set_float(param, mpack_expect_float(reader));
		break;
	case PARAM_TYPE_DOUBLE:
		param_set_double(param, mpack_expect_double(reader));
		break;

	case PARAM_TYPE_STRING:
	{
		int count = mpack_expect_str(reader);
		param_set_string(param, reader->buffer + reader->pos, count);
		reader->pos += count;
        reader->left -= count;
		mpack_done_str(reader);
		break;
	}
	case PARAM_TYPE_DATA:
	{
		int count = mpack_expect_bin(reader);
		param_set_data(param, reader->buffer + reader->pos, count);
		reader->pos += count;
		reader->left -= count;
		mpack_done_bin(reader);
		break;
	}

	default:
	case PARAM_TYPE_VECTOR3:
		mpack_discard(reader);
		break;
	}

}
