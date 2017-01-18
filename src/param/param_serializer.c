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
