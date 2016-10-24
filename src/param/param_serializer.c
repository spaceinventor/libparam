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
#include "param_serializer.h"
#include "param_string.h"

#include <csp/csp_endian.h>

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

#undef PARAM_DESERIALIZE

		case PARAM_TYPE_DATA:
		case PARAM_TYPE_STRING:
			memcpy(out, in, size);
			count += size;
			break;

	}

	return count;

}

int param_deserialize_to_param(void * in, param_t * param) {

	int count = 0;

	switch(param->type) {

#define PARAM_DESERIALIZE(_case, _type, _name) \
		case _case: { \
			_type obj; \
			count += param_deserialize_to_var(param->type, param->size, in, &obj); \
			param_set_##_name(param, obj); \
			break; \
		}

		PARAM_DESERIALIZE(PARAM_TYPE_UINT8, uint8_t, uint8)
		PARAM_DESERIALIZE(PARAM_TYPE_UINT16, uint16_t, uint16)
		PARAM_DESERIALIZE(PARAM_TYPE_UINT32, uint32_t, uint32)
		PARAM_DESERIALIZE(PARAM_TYPE_UINT64, uint64_t, uint64)
		PARAM_DESERIALIZE(PARAM_TYPE_INT8, int8_t, uint8)
		PARAM_DESERIALIZE(PARAM_TYPE_INT16, int16_t, uint16)
		PARAM_DESERIALIZE(PARAM_TYPE_INT32, int32_t, uint32)
		PARAM_DESERIALIZE(PARAM_TYPE_INT64, int64_t, uint64)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT8, uint8_t, uint8)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT16, uint16_t, uint16)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT32, uint32_t, uint32)
		PARAM_DESERIALIZE(PARAM_TYPE_XINT64, uint64_t, uint64)
		PARAM_DESERIALIZE(PARAM_TYPE_FLOAT, float, float)
		PARAM_DESERIALIZE(PARAM_TYPE_DOUBLE, double, double)

		case PARAM_TYPE_STRING:
		case PARAM_TYPE_DATA:
			printf("Set data\n");
			param_set_data(param, in, param->size);
			count += param->size;
			break;

#undef PARAM_DESERIALIZE
	}

	return count;

}

int param_serialize_from_str(param_type_e type, char * str, void * out, int strlen)
{

	int count = 0;

	switch(type) {

#define PARAM_SERIALIZE(casename, objtype, name, swapfct) \
		case casename: { \
			objtype obj; \
			param_str_to_value(type, str, &obj); \
			obj = swapfct(obj); \
			memcpy(out, &obj, sizeof(objtype)); \
			count += sizeof(objtype); \
			break; \
		}

		PARAM_SERIALIZE(PARAM_TYPE_UINT8, uint8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_UINT16, uint16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_UINT32, uint32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_UINT64, uint64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_INT8, int8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_INT16, int16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_INT32, int32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_INT64, int64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_XINT16, uint16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_XINT32, uint32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_XINT64, uint64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_FLOAT, float, float, )
		PARAM_SERIALIZE(PARAM_TYPE_DOUBLE, double, double, )

		case PARAM_TYPE_STRING:
			strncpy(out, str, strlen);
			count += strlen;
			break;

		default:
			printf("parameter type not supported\r\n");
			break;

#undef PARAM_SERIALIZE
	}

	return count;

}

int param_serialize_from_param(param_t * param, char * out)
{

	int count = 0;

	switch(param->type) {

#define PARAM_SERIALIZE(casename, type, name, swapfct) \
		case casename: { \
			type obj = param_get_##name(param); \
			obj = swapfct(obj); \
			memcpy(out, &obj, sizeof(type)); \
			count += sizeof(type); \
			break; \
		}

		PARAM_SERIALIZE(PARAM_TYPE_UINT8, uint8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_UINT16, uint16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_UINT32, uint32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_UINT64, uint64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_INT8, int8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_INT16, int16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_INT32, int32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_INT64, int64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_XINT16, uint16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_XINT32, uint32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_XINT64, uint64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_FLOAT, float, float, )
		PARAM_SERIALIZE(PARAM_TYPE_DOUBLE, double, double, )

		case PARAM_TYPE_STRING:
		case PARAM_TYPE_DATA:
			param_get_data(param, out, param->size);
			count += param->size;
			break;


#undef PARAM_SERIALIZE
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
		PARAM_SERIALIZE(PARAM_TYPE_INT8, int8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_INT16, int16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_INT32, int32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_INT64, int64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_XINT8, uint8_t, uint8, )
		PARAM_SERIALIZE(PARAM_TYPE_XINT16, uint16_t, uint16, csp_hton16)
		PARAM_SERIALIZE(PARAM_TYPE_XINT32, uint32_t, uint32, csp_hton32)
		PARAM_SERIALIZE(PARAM_TYPE_XINT64, uint64_t, uint64, csp_hton64)
		PARAM_SERIALIZE(PARAM_TYPE_FLOAT, float, float, )
		PARAM_SERIALIZE(PARAM_TYPE_DOUBLE, double, double, )

		case PARAM_TYPE_STRING:
		case PARAM_TYPE_DATA:
			memcpy(out, in, size);
			count += size;
			break;

#undef PARAM_SERIALIZE
	}

	return count;

}

int param_deserialize_single(char * inbuf) {

	int count = 0;

	uint16_t idx;
	memcpy(&idx, inbuf, sizeof(idx));
	count += sizeof(idx);
	idx = csp_ntoh16(idx);

	param_t * param = param_index_to_ptr(idx);

	count += param_deserialize_to_param(inbuf + count, param);

	return count;

}

int param_serialize_single_fromstr(uint16_t idx, param_type_e type, char * in, char * outbuf, int len) {

	int size = 0;

	/* Parameter id */
	idx = csp_hton16(idx);
	memcpy(outbuf, &idx, sizeof(uint16_t));
	size += sizeof(uint16_t);

	size += param_serialize_from_str(type, in, outbuf + size, len);

	return size;

}

int param_serialize_single(param_t * param, char * outbuf, int len)
{

	int size = 0;

	/* Parameter id */
	uint16_t idx = param_ptr_to_index(param);
	//printf("paramter %s type %u idx %u\r\n", param->name, param->type, (unsigned int) idx);
	idx = csp_hton16(idx);
	memcpy(outbuf, &idx, sizeof(uint16_t));
	size += sizeof(uint16_t);

	size += param_serialize_from_param(param, outbuf + size);

	return size;
}

int param_serialize(param_t * param[], int count, char * outbuf, int len)
{
	int output = 0;
	for (int i = 0; i < count; i++) {
		output += param_serialize_single(param[i], outbuf + output, len - output);
		if (output >= len)
			return output;
	}
	return output;
}

int param_serialize_idx(uint16_t param_idx[], int count, char * outbuf, int len)
{
	param_t * param[count];
	for (int i = 0; i < count; i++) {
		param[i] = param_index_to_ptr(param_idx[i]);
	}
	return param_serialize(param, count, outbuf, len);
}
