/*
 * param_serializer.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <hex_dump.h>

#include <param.h>
#include "param_serializer.h"
#include "param_string.h"

#include <csp/csp_endian.h>

int param_deserialize_single(char * inbuf) {

	int count = 0;

	uint16_t idx;
	memcpy(&idx, inbuf, sizeof(idx));
	count += sizeof(idx);
	idx = csp_ntoh16(idx);

	param_t * param = param_index_to_ptr(idx);

	switch(param->type) {

#define PARAM_DESERIALIZE(casename, type, name, swapfct) \
		case casename: { \
			type obj; \
			memcpy(&obj, inbuf + count, sizeof(obj)); \
			count += sizeof(obj); \
			obj = swapfct(obj); \
			param_set_##name(param, obj); \
			break; \
		}

		PARAM_DESERIALIZE(PARAM_TYPE_UINT8, uint8_t, uint8, )
		PARAM_DESERIALIZE(PARAM_TYPE_UINT16, uint16_t, uint16, csp_ntoh16)
		PARAM_DESERIALIZE(PARAM_TYPE_UINT32, uint32_t, uint32, csp_ntoh32)
		PARAM_DESERIALIZE(PARAM_TYPE_UINT64, uint64_t, uint64, csp_ntoh64)
		PARAM_DESERIALIZE(PARAM_TYPE_INT8, int8_t, uint8, )
		PARAM_DESERIALIZE(PARAM_TYPE_INT16, int16_t, uint16, csp_ntoh16)
		PARAM_DESERIALIZE(PARAM_TYPE_INT32, int32_t, uint32, csp_ntoh32)
		PARAM_DESERIALIZE(PARAM_TYPE_INT64, int64_t, uint64, csp_ntoh64)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_DESERIALIZE(PARAM_TYPE_XINT16, uint16_t, uint16, csp_ntoh16)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT32, uint32_t, uint32, csp_ntoh32)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT64, uint64_t, uint64, csp_ntoh64)
		PARAM_DESERIALIZE(PARAM_TYPE_FLOAT, float, float, )
		PARAM_DESERIALIZE(PARAM_TYPE_DOUBLE, double, double, )
		default:
			printf("Unuspported type\r\n");
			break;
	}

	return count;

}

int param_serialize_single_fromstr(uint16_t idx, param_type_e type, char * in, char * outbuf, int len) {

	int size = 0;

	/* Parameter id */
	idx = csp_hton16(idx);
	memcpy(outbuf, &idx, sizeof(uint16_t));
	size += sizeof(uint16_t);

	/* Value */
	switch(type) {

#define PARAM_SWITCH_MEMCPY(casename, objtype, name, swapfct) \
		case casename: { \
			objtype obj; \
			param_str_to_value(type, in, &obj); \
			obj = swapfct(obj); \
			memcpy(outbuf + size, &obj, sizeof(objtype)); \
			size += sizeof(objtype); \
			break; \
		}

		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT8, uint8_t, uint8, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT16, uint16_t, uint16, csp_hton16)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT32, uint32_t, uint32, csp_hton32)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT64, uint64_t, uint64, csp_hton64)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT8, int8_t, uint8, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT16, int16_t, uint16, csp_hton16)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT32, int32_t, uint32, csp_hton32)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT64, int64_t, uint64, csp_hton64)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT16, uint16_t, uint16, csp_hton16)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT32, uint32_t, uint32, csp_hton32)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT64, uint64_t, uint64, csp_hton64)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_FLOAT, float, float, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_DOUBLE, double, double, )
		default:
			printf("parameter type not supported\r\n");
			break;

#undef PARAM_SWITCH_MEMCPY
	}

	return size;

}

int param_serialize_single(param_t * param, char * outbuf, int len)
{

	int size = 0;

	/* Parameter id */
	uint16_t idx = param_ptr_to_index(param);
	printf("paramter %s type %u idx %u\r\n", param->name, param->type, (unsigned int) idx);
	idx = csp_hton16(idx);
	memcpy(outbuf, &idx, sizeof(uint16_t));
	size += sizeof(uint16_t);

	/* Value */
	switch(param->type) {

#define PARAM_SWITCH_MEMCPY(casename, type, name, swapfct) \
		case casename: { \
			type obj = param_get_##name(param); \
			obj = swapfct(obj); \
			memcpy(outbuf + size, &obj, sizeof(type)); \
			size += sizeof(type); \
			break; \
		}

		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT8, uint8_t, uint8, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT16, uint16_t, uint16, csp_hton16)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT32, uint32_t, uint32, csp_hton32)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_UINT64, uint64_t, uint64, csp_hton64)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT8, int8_t, uint8, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT16, int16_t, uint16, csp_hton16)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT32, int32_t, uint32, csp_hton32)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_INT64, int64_t, uint64, csp_hton64)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT16, uint16_t, uint16, csp_hton16)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT32, uint32_t, uint32, csp_hton32)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_XINT64, uint64_t, uint64, csp_hton64)
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_FLOAT, float, float, )
		PARAM_SWITCH_MEMCPY(PARAM_TYPE_DOUBLE, double, double, )
		default:
			printf("parameter type not supported\r\n");
			break;
#undef PARAM_SWITCH_MEMCPY
	}

	printf("Param %s\r\n", param->name);
	return size;
}

void param_serialize(param_t * param[], int count, char * outbuf, int len)
{
	int output = 0;
	for (int i = 0; i < count; i++) {
		output += param_serialize_single(param[i], outbuf + output, len - output);
		if (output >= len)
			return;
	}
}

void param_serialize_idx(uint16_t param_idx[], int count, char * outbuf, int len)
{
	param_t * param[count];
	for (int i = 0; i < count; i++) {
		param[i] = param_index_to_ptr(param_idx[i]);
	}
	param_serialize(param, count, outbuf, len);
}
