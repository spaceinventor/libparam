#include <stdio.h>
#include <string.h>
#include <param/param.h>
#include "param_string.h"

#include <csp/csp_endian.h>

extern param_t __start_param, __stop_param;

#ifndef PARAM_STORAGE_SIZE
#define PARAM_STORAGE_SIZE sizeof(param_t)
#endif

#define for_each_param(_c) \
	for (_c = &__start_param; \
	     _c < &__stop_param; \
	     _c = (param_t *)(intptr_t)((char *)_c + PARAM_STORAGE_SIZE))

/* Callbacks on/off */
static bool param_callbacks_enabled = true;

void param_callback_enabled(bool callbacks_enabled)
{
	param_callbacks_enabled = callbacks_enabled;
}

param_t * param_from_id(uint16_t id)
{
	return (param_t *) &__start_param + id;
}

void param_print(param_t * param)
{
	if (param == NULL)
		return;
	if (param >= &__stop_param)
		return;
	if (param < &__start_param)
		return;

	/* Param id */
	printf(" %u",  param_ptr_to_index(param));

	/* Vmem */
#if 0
	if (param->vmem) {
		printf(" %s", param->vmem->name);
	} else {
		printf(" RAM");
	}
#endif

	/* Name and value */
	char value_str[20] = "";
	param_value_str(param, value_str, 20);
	printf(" %s = %s", param->name, value_str);

	/* Unit */
	printf(" %s", param->unit);

	/* Type */
	char type_str[20] = "";
	param_type_str(param, type_str, 20);
	printf(" %s", type_str);

	printf("\n");

}

void param_list(struct vmem_s * vmem)
{
	param_t * param;
	for_each_param(param) {

		/* Filter on vmem */
		if ((vmem != NULL) && (param->vmem != vmem))
			continue;

		param_print(param);
	}
}

void param_list_array(param_t * param, int count)
{
	for (int i = 0; i < count; i++) {
		param_print(param + i);
	}
}

#define PARAM_GET(_type, _name, _swapfct) \
	_type param_get_##_name(param_t * param) { \
		/* Aligned access directly to RAM */ \
		if (param->physaddr) \
			return *(_type *)(param->physaddr); \
		\
		_type data; \
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

#define PARAM_SET(_type, name_in, _swapfct) \
	void param_set_##name_in(param_t * param, _type value) \
	{ \
	\
	/* Check readonly */ \
	if ((param->readonly == PARAM_READONLY_TRUE) || (param->readonly == PARAM_READONLY_INTERNAL)) { \
		printf("Tried to set readonly parameter %s\r\n", param->name); \
		return; \
	} \
	\
	/* Check limits */ \
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
	if ((param_callbacks_enabled == true) && (param->callback)) { \
		param->callback(param); \
	} \
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
	default:
		printf("Unsupported type\n");
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

param_t * param_name_to_ptr(char * name)
{
	param_t * param;
	for_each_param(param) {
		if (strcmp(param->name, name) == 0) {
			return param;
		}
	}
	return NULL;
}

param_t * param_index_to_ptr(int idx)
{
	return (param_t *) (((char *) &__start_param) + idx * PARAM_STORAGE_SIZE);
}

int param_ptr_to_index(param_t * param)
{
	return ((intptr_t) param - (intptr_t) &__start_param) / PARAM_STORAGE_SIZE;
}

