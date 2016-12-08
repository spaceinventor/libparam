#include <stdio.h>
#include <string.h>
#include <param/param.h>
#include "param_string.h"

#include <csp/csp.h>
#include <csp/csp_endian.h>

void param_print(param_t * param)
{
	if (param == NULL)
		return;
	if (param >= &__stop_param)
		return;
	if (param < &__start_param)
		return;

	/* Param id */
	printf(" %u", param->id);

	/* Vmem */
#if 0
	if (param->vmem) {
		printf(" %s", param->vmem->name);
	} else {
		printf(" RAM");
	}
#endif

	/* Name and value */
	char value_str[41] = {};
	param_value_str(param, value_str, 40);
	printf(" %s = %s", param->name, value_str);

	/* Unit */
	if (param->unit != NULL)
		printf(" %s", param->unit);

	/* Type */
	char type_str[11] = {};
	param_type_str(param->type, type_str, 10);
	printf(" %s", type_str);

	printf("\n");

}

void param_list(char * token)
{
	param_t * param;

	param_foreach(param) {

		if (param->readonly == PARAM_HIDDEN)
			continue;

#define param_min(a,b) \
	({ __typeof__ (a) _a = (a); \
	__typeof__ (b) _b = (b); \
	_a < _b ? _a : _b; })

		if (token)
			if (strncmp(token, param->name, param_min(strlen(param->name), strlen(token))) != 0)
				continue;

		param_print(param);
	}
}

#define PARAM_GET(_type, _name, _swapfct) \
	_type param_get_##_name(param_t * param) { \
		/* Aligned access directly to RAM */ \
		if (param->physaddr) \
			return *(_type *)(param->physaddr); \
		\
		_type data = 0; \
		param->vmem->read(param->vmem, param->addr, &data, sizeof(data)); \
		if (param->vmem->big_endian == 1) { \
			data = _swapfct(data); \
		} \
		return data; \
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

void param_get_data(param_t * param, void * outbuf, int len)
{
	if (param->physaddr) {
		memcpy(outbuf, param->physaddr, len);
		return;
	}
	param->vmem->read(param->vmem, param->addr, outbuf, len);
}

#if 0
/* Check limits : Fixme: does not work sig signed integers */ \
if ((param->type != PARAM_TYPE_FLOAT) && (param->type != PARAM_TYPE_DOUBLE)) { \
	if (value > (_type) param->max) { \
		printf("Param value exceeds max\r\n"); \
		return; \
	} \
	\
	if (value < (_type) param->min) { \
		printf("Param value below min\r\n"); \
		return; \
	} \
}
#endif

#define PARAM_SET(_type, name_in, _swapfct) \
	void __param_set_##name_in(param_t * param, _type value, bool do_callback); \
	void __param_set_##name_in(param_t * param, _type value, bool do_callback) \
	{ \
		\
		/* Check readonly */ \
		if ((param->readonly == PARAM_READONLY_TRUE) || (param->readonly == PARAM_READONLY_INTERNAL)) { \
			printf("Tried to set readonly parameter %s\r\n", param->name); \
			return; \
		} \
		\
		/* Aligned access directly to RAM */ \
		if (param->physaddr) { \
			*(_type*)(param->physaddr) = value; \
		\
		/* Otherwise call to vmem */ \
		} else { \
			if (param->vmem->big_endian == 1) \
				value = _swapfct(value); \
			param->vmem->write(param->vmem, param->addr, &value, sizeof(value)); \
		} \
		\
		/* Callback */ \
		if ((do_callback == true) && (param->callback)) { \
			param->callback(param); \
		} \
	} \
	inline void param_set_##name_in(param_t * param, _type value) \
	{ \
		__param_set_##name_in(param, value, true); \
	} \
	inline void param_set_##name_in##_nocallback(param_t * param, _type value) \
	{ \
		__param_set_##name_in(param, value, false); \
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

void param_set(param_t * param, void * value) {
	switch(param->type) {

#define PARAM_SET(casename, name, type) \
	case casename: \
		param_set_##name(param, *(type *) value); \
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
	if (param->physaddr) {
		memcpy(param->physaddr, inbuf, len);
		return;
	}
	param->vmem->write(param->vmem, param->addr, inbuf, len);
}

param_t * param_ptr_from_name(char * name)
{
	param_t * param;
	param_foreach(param) {
		if (strcmp(param->name, name) == 0) {
			return param;
		}
	}
	return NULL;
}

param_t * param_ptr_from_id(int id) {

	param_t * param;
	param_foreach(param) {
		if (param->id == id)
			return param;
	}

	return NULL;
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
	case PARAM_TYPE_STRING: return -1; break;
	case PARAM_TYPE_DATA: return -1; break;
	case PARAM_TYPE_VECTOR3: return sizeof(param_type_vector3); break;
	}
	return -1;
}

