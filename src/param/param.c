#include <stdio.h>
#include <string.h>
#include <param/param.h>
#include <libparam.h>

#include <csp/csp.h>
#include <csp/csp_endian.h>

#define PARAM_GET(_type, _name, _swapfct) \
	_type param_get_##_name##_array(param_t * param, unsigned int i) { \
		if (i > param->size) { \
			return 0; \
		} \
		switch(param->storage_type) {\
		case PARAM_STORAGE_RAM: \
			if (param->physaddr) \
				return *(_type *)(param->physaddr + i * sizeof(_type)); \
			return 0; \
		case PARAM_STORAGE_VMEM: { \
			_type data = 0; \
			param->vmem->read(param->vmem, param->addr + i * sizeof(_type), &data, sizeof(data)); \
			if (param->vmem->big_endian == 1) { \
				data = _swapfct(data); \
			} \
			return data; \
		} \
		case PARAM_STORAGE_REMOTE: \
			if (param->value_get) \
				return *(_type *)(param->value_get + i * sizeof(_type)); \
			return 0; \
		case PARAM_STORAGE_TEMPLATE: \
			return 0; \
		} \
		return 0; \
	} \
	_type param_get_##_name(param_t * param) { \
		return param_get_##_name##_array(param, 0); \
	}

PARAM_GET(uint8_t, uint8, )
PARAM_GET(uint16_t, uint16, csp_betoh16)
PARAM_GET(uint32_t, uint32, csp_betoh32)
PARAM_GET(uint64_t, uint64, csp_betoh64)
PARAM_GET(int8_t, int8, )
PARAM_GET(int16_t, int16, csp_betoh16)
PARAM_GET(int32_t, int32, csp_betoh32)
PARAM_GET(int64_t, int64, csp_betoh64)
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
		param_get_data(param, value, param->size);
		break;
	case PARAM_TYPE_VECTOR3:
	case PARAM_TYPE_DATA:
		param_get_data(param, value, param->size);
		break;

	}
}

void param_get_data(param_t * param, void * outbuf, int len)
{
	switch(param->storage_type) {
	case PARAM_STORAGE_RAM:
		if (param->physaddr)
			memcpy(outbuf, param->physaddr, len);
		return;
	case PARAM_STORAGE_VMEM:
		param->vmem->read(param->vmem, param->addr, outbuf, len);
		return;
	case PARAM_STORAGE_REMOTE:
		if (param->value_get)
			memcpy(outbuf, param->value_get, len);
		return;
	case PARAM_STORAGE_TEMPLATE:
		return;
	}
}

#ifndef PARAM_LOG
#define param_log(...)
#endif

#define PARAM_SET(_type, name_in, _swapfct) \
	void __param_set_##name_in(param_t * param, _type value, bool do_callback, unsigned int i) { \
		if (i > param->size) { \
			return; \
		} \
		if (param->storage_type == PARAM_STORAGE_REMOTE) { \
			if (param->value_get) { \
				*(_type *) (param->value_get + i * sizeof(_type)) = value; \
			} \
			return; \
		} \
		\
		/* Check readonly */ \
		if ((param->readonly == PARAM_READONLY_TRUE) || (param->readonly == PARAM_READONLY_INTERNAL)) { \
			printf("Tried to set readonly parameter %s\r\n", param->name); \
			return; \
		} \
		/* Log */ \
		param_log(param, &value); \
		\
		/* Aligned access directly to RAM */ \
		if (param->storage_type == PARAM_STORAGE_RAM) { \
			if (param->physaddr) \
				*(_type*)(param->physaddr + i * sizeof(_type)) = value; \
		} \
		\
		/* Otherwise call to vmem */ \
		if (param->storage_type == PARAM_STORAGE_VMEM) { \
			if (param->vmem->big_endian == 1) \
				value = _swapfct(value); \
			param->vmem->write(param->vmem, param->addr + i * sizeof(_type), &value, sizeof(_type)); \
		} \
		\
		/* Callback */ \
		if ((do_callback == true) && (param->callback)) { \
			param->callback(param); \
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
PARAM_SET(uint16_t, uint16, csp_htobe16)
PARAM_SET(uint32_t, uint32, csp_htobe32)
PARAM_SET(uint64_t, uint64, csp_htobe64)
PARAM_SET(int8_t, int8, )
PARAM_SET(int16_t, int16, csp_htobe16)
PARAM_SET(int32_t, int32, csp_htobe32)
PARAM_SET(int64_t, int64, csp_htobe64)
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
	case PARAM_TYPE_VECTOR3:
	case PARAM_TYPE_DATA:
		param_set_data(param, value, param->size);
		break;

	}
}

void param_set_data(param_t * param, void * inbuf, int len) {
	switch(param->storage_type) {
	case PARAM_STORAGE_RAM:
		if (param->physaddr) {
			memcpy(param->physaddr, inbuf, len);
			if ((param->type == PARAM_TYPE_STRING) && (len < param->size)) {
				((char *) param->physaddr)[len] = '\0';
			}
		}
		return;
	case PARAM_STORAGE_VMEM:
		param->vmem->write(param->vmem, param->addr, inbuf, len);
		return;
	case PARAM_STORAGE_REMOTE:
		if (param->value_get) {
			memcpy(param->value_get, inbuf, len);
			if ((param->type == PARAM_TYPE_STRING) && (len < param->size)) {
				((char *) param->value_get)[len] = '\0';
			}
		}
		return;
	case PARAM_STORAGE_TEMPLATE:
		return;
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
	case PARAM_TYPE_VECTOR3: return sizeof(param_type_vector3); break;
	}
	return -1;
}

int param_size(param_t * param) {
	switch(param->type) {
	case PARAM_TYPE_STRING:
	case PARAM_TYPE_DATA:
		return param->size;
	default:
		return param_typesize(param->type);
	}
}

