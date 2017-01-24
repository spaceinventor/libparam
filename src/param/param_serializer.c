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

int param_serialize_chunk_timestamp(uint32_t timestamp, uint8_t * out) {
	timestamp = csp_hton32(timestamp);
	out[0] = PARAM_CHUNK_TIME;
	memcpy(&out[1], &timestamp, sizeof(timestamp));
	return 1 + sizeof(timestamp);
}

int param_deserialize_chunk_timestamp(uint32_t * timestamp, uint8_t * in) {
	memcpy(timestamp, &in[1], sizeof(*timestamp));
	*timestamp = csp_ntoh32(*timestamp);
	return 1 + sizeof(*timestamp);
}

int param_serialize_chunk_node(uint8_t node, uint8_t * out) {
	out[0] = PARAM_CHUNK_NODE;
	out[1] = node;
	return 1 + sizeof(node);
}

int param_deserialize_chunk_node(uint8_t * node, uint8_t * in) {
	*node = in[1];
	return 1 + sizeof(*node);
}

int param_serialize_chunk_param(param_t * param, uint8_t * out) {
	out[0] = PARAM_CHUNK_PARAM;
	uint16_t param_net = csp_hton16(param->id);
	memcpy(&out[1], &param, sizeof(param_net));
	return 1 + sizeof(param_net);
}

int param_serialize_chunk_params_begin(uint8_t ** count, uint8_t * out) {
	out[0] = PARAM_CHUNK_PARAMS;
	out[1] = 0;
	*count = &out[1];
	return 2;
}

int param_serialize_chunk_params_next(param_t * param, uint8_t * count, uint8_t * out) {
	*count = *count + 1;
	uint16_t param_net = csp_hton16(param->id);
	memcpy(out, &param_net, sizeof(param_net));
	return sizeof(param_net);
}

int param_deserialize_chunk_params_begin(uint8_t * count, uint8_t * in) {
	*count = in[1];
	return 1 + sizeof(*count);
}

int param_deserialize_chunk_params_next(uint16_t * paramid, uint8_t * in) {
	memcpy(paramid, in, sizeof(*paramid));
	*paramid = csp_ntoh16(*paramid);
	return sizeof(*paramid);
}

int param_serialize_chunk_param_and_value(param_t * params[], uint8_t count, uint8_t * out, int pending_only) {

	out[0] = PARAM_CHUNK_PARAM_AND_VALUE;
	out[1] = 0;

	int outset = 2;
	for (int i = i; i < count; i++) {

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
			param_get(params[i], tmp);
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
			param_set(param, tmp);
		}

		if (verbose)
			param_print(param);

		/**
		 * TODO: Param log assumes ordered input
		 */
		//param_log(param, param->value_get, timestamp);

	}
	return inset;
}
