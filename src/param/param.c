#include <string.h>
#include <param/param.h>
#include <libparam.h>

#include <csp/csp.h>
#include <sys/types.h>

#define PARAM_GET(_type, _name, _swapfct) \
	_type param_get_##_name##_array(param_t * param, unsigned int i) { \
		if (i > (unsigned int) param->array_size) { \
			return 0; \
		} \
		if (param->vmem) { \
			_type data = 0; \
			param->vmem->read(param->vmem, (uint32_t) (intptr_t) param->addr + i * param->array_step, &data, sizeof(data)); \
			if (param->vmem->big_endian == 1) { \
				data = _swapfct(data); \
			} \
			return data; \
		} else { \
			return *(_type *)(param->addr + i * param->array_step); \
		} \
	} \
	_type param_get_##_name(param_t * param) { \
		return param_get_##_name##_array(param, 0); \
	}

PARAM_GET(uint8_t, uint8, )
PARAM_GET(uint16_t, uint16, be16toh)
PARAM_GET(uint32_t, uint32, be32toh)
PARAM_GET(uint64_t, uint64, be64toh)
PARAM_GET(int8_t, int8, )
PARAM_GET(int16_t, int16, be16toh)
PARAM_GET(int32_t, int32, be32toh)
PARAM_GET(int64_t, int64, be64toh)
PARAM_GET(float, float, )
PARAM_GET(double, double, )

#undef PARAM_GET

void param_get(param_t * param, unsigned int offset, void * value) {
	switch(param->type) {

#define PARAM_GET(casename, name, type) \
	case casename: \
		*(type *) value = param_get_##name##_array(param, offset); \
		break; \

	PARAM_GET(PARAM_TYPE_UINT8, uint8, uint8_t)
	PARAM_GET(PARAM_TYPE_UINT16, uint16, uint16_t)
	PARAM_GET(PARAM_TYPE_UINT32, uint32, uint32_t)
	PARAM_GET(PARAM_TYPE_UINT64, uint64, uint64_t)
	PARAM_GET(PARAM_TYPE_INT8, int8, int8_t)
	PARAM_GET(PARAM_TYPE_INT16, int16, int16_t)
	PARAM_GET(PARAM_TYPE_INT32, int32, int32_t)
	PARAM_GET(PARAM_TYPE_INT64, int64, int64_t)
	PARAM_GET(PARAM_TYPE_XINT8, uint8, uint8_t)
	PARAM_GET(PARAM_TYPE_XINT16, uint16, uint16_t)
	PARAM_GET(PARAM_TYPE_XINT32, uint32, uint32_t)
	PARAM_GET(PARAM_TYPE_XINT64, uint64, uint64_t)
	PARAM_GET(PARAM_TYPE_FLOAT, float, float)
	PARAM_GET(PARAM_TYPE_DOUBLE, double, double)
	case PARAM_TYPE_STRING:
	case PARAM_TYPE_DATA:
		param_get_data(param, value, param->array_size);
		break;
    case PARAM_TYPE_INVALID:
        break;     
	}
    
}

void param_get_data(param_t * param, void * outbuf, int len)
{
	if (param->vmem) {
		param->vmem->read(param->vmem, (uint32_t) (intptr_t) param->addr, outbuf, len);
	} else {
		memcpy(outbuf, param->addr, len);
	}
}

#ifndef PARAM_LOG
#define param_log(...)
#endif

#define PARAM_SET(_type, name_in, _swapfct) \
	void __param_set_##name_in(param_t * param, _type value, bool do_callback, unsigned int i) { \
		if (i > (unsigned int) param->array_size) { \
			return; \
		} \
		if (param->vmem) { \
			if (param->vmem->big_endian == 1) \
				value = _swapfct(value); \
			param->vmem->write(param->vmem, (uint32_t) (intptr_t) param->addr + i * param->array_step, &value, sizeof(_type)); \
		} else { \
			/* Aligned access directly to RAM */ \
			*(_type*)(param->addr + i * param->array_step) = value; \
		} \
		/* Callback */ \
		if ((do_callback == true) && (param->callback)) { \
			param->callback(param, i); \
		} \
	} \
	inline void param_set_##name_in(param_t * param, _type value) \
	{ \
		__param_set_##name_in(param, value, true, 0); \
	} \
	inline void param_set_##name_in##_nocallback(param_t * param, _type value) \
	{ \
		__param_set_##name_in(param, value, false, 0); \
	} \
	inline void param_set_##name_in##_array(param_t * param, unsigned int i, _type value) \
	{ \
		__param_set_##name_in(param, value, true, i); \
	} \
	inline void param_set_##name_in##_array_nocallback(param_t * param, unsigned int i, _type value) \
	{ \
		__param_set_##name_in(param, value, false, i); \
	}

PARAM_SET(uint8_t, uint8, )
PARAM_SET(uint16_t, uint16, htobe16)
PARAM_SET(uint32_t, uint32, htobe32)
PARAM_SET(uint64_t, uint64, htobe64)
PARAM_SET(int8_t, int8, )
PARAM_SET(int16_t, int16, htobe16)
PARAM_SET(int32_t, int32, htobe32)
PARAM_SET(int64_t, int64, htobe64)
PARAM_SET(float, float, )
PARAM_SET(double, double, )

#undef PARAM_SET

void param_set(param_t * param, unsigned int offset, void * value) {
	switch(param->type) {

#define PARAM_SET(casename, name, type) \
	case casename: \
		param_set_##name##_array(param, offset, *(type *) value); \
		break; \

	PARAM_SET(PARAM_TYPE_UINT8, uint8, uint8_t)
	PARAM_SET(PARAM_TYPE_UINT16, uint16, uint16_t)
	PARAM_SET(PARAM_TYPE_UINT32, uint32, uint32_t)
	PARAM_SET(PARAM_TYPE_UINT64, uint64, uint64_t)
	PARAM_SET(PARAM_TYPE_INT8, int8, int8_t)
	PARAM_SET(PARAM_TYPE_INT16, int16, int16_t)
	PARAM_SET(PARAM_TYPE_INT32, int32, int32_t)
	PARAM_SET(PARAM_TYPE_INT64, int64, int64_t)
	PARAM_SET(PARAM_TYPE_XINT8, uint8, uint8_t)
	PARAM_SET(PARAM_TYPE_XINT16, uint16, uint16_t)
	PARAM_SET(PARAM_TYPE_XINT32, uint32, uint32_t)
	PARAM_SET(PARAM_TYPE_XINT64, uint64, uint64_t)
	PARAM_SET(PARAM_TYPE_FLOAT, float, float)
	PARAM_SET(PARAM_TYPE_DOUBLE, double, double)
	case PARAM_TYPE_STRING:
		param_set_data(param, value, strlen(value) + 1);
		break;
	case PARAM_TYPE_DATA:
		param_set_data(param, value, param->array_size);
		break;
    case PARAM_TYPE_INVALID:
        break;
	}
}

void param_set_string(param_t * param, void * inbuf, int len) {
	param_set_data(param, inbuf, len);
	/* Termination */
	if (param->vmem) {
		param->vmem->write(param->vmem, (uint32_t) (intptr_t) param->addr + len, "", 1);
	} else {
		memcpy(param->addr + len , "", 1);
	}
	/* Callback */
	if (param->callback) {
		param->callback(param, 0);
	}
}

void param_set_data_nocallback(param_t * param, void * inbuf, int len) {
	if (param->vmem) {
		param->vmem->write(param->vmem, (uint32_t) (intptr_t) param->addr, inbuf, len);
	} else {
		memcpy(param->addr, inbuf, len);
	}
}

void param_set_data(param_t * param, void * inbuf, int len) {
	param_set_data_nocallback(param, inbuf, len);
	/* Callback */
	if (param->callback) {
		param->callback(param, 0);
	}
}

int param_typesize(param_type_e type) {
	switch(type) {
	case PARAM_TYPE_UINT8: return sizeof(uint8_t); break;
	case PARAM_TYPE_UINT16: return sizeof(uint16_t); break;
	case PARAM_TYPE_UINT32: return sizeof(uint32_t); break;
	case PARAM_TYPE_UINT64: return sizeof(uint64_t); break;
	case PARAM_TYPE_INT8: return sizeof(int8_t); break;
	case PARAM_TYPE_INT16: return sizeof(int16_t); break;
	case PARAM_TYPE_INT32: return sizeof(int32_t); break;
	case PARAM_TYPE_INT64: return sizeof(int64_t); break;
	case PARAM_TYPE_XINT8: return sizeof(uint8_t); break;
	case PARAM_TYPE_XINT16: return sizeof(uint16_t); break;
	case PARAM_TYPE_XINT32: return sizeof(uint32_t); break;
	case PARAM_TYPE_XINT64: return sizeof(uint64_t); break;
	case PARAM_TYPE_FLOAT: return sizeof(float); break;
	case PARAM_TYPE_DOUBLE: return sizeof(double); break;
	case PARAM_TYPE_STRING: return 1; break;
	case PARAM_TYPE_DATA: return 1; break;
    case PARAM_TYPE_INVALID: return 0; break;
	}
	return -1;
}

int param_size(param_t * param) {
	switch(param->type) {
	case PARAM_TYPE_STRING:
	case PARAM_TYPE_DATA:
		return param->array_size;
	default:
		return param_typesize(param->type);
	}
}

void param_copy(param_t * dest, param_t * src) {

	/* Type check */
	if (dest->type != src->type) {
		return;
	}

	switch(dest->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_INT8:
		case PARAM_TYPE_XINT8:
			param_set_uint8(dest, param_get_uint8(src));
			break;
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_INT16:
		case PARAM_TYPE_XINT16:
			param_set_uint16(dest, param_get_uint16(src));
			break;
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_INT32:
		case PARAM_TYPE_XINT32:
			param_set_uint32(dest, param_get_uint32(src));
			break;
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_INT64:
		case PARAM_TYPE_XINT64:
			param_set_uint64(dest, param_get_uint64(src));
			break;
		case PARAM_TYPE_FLOAT:
			param_set_float(dest, param_get_float(src));
			break;
		case PARAM_TYPE_DOUBLE:
			param_set_double(dest, param_get_double(src));
			break;
		case PARAM_TYPE_STRING: {
			char stack_buffer[dest->array_size];
			param_get_string(src, stack_buffer, dest->array_size);
			param_set_string(dest, stack_buffer, dest->array_size);
			break;
		}
		case PARAM_TYPE_DATA: {
			char stack_buffer[dest->array_size];
			param_get_data(src, stack_buffer, dest->array_size);
			param_set_data(dest, stack_buffer, dest->array_size);
			break;
		}
        case PARAM_TYPE_INVALID:
            break;
	}

}


