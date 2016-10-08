/*
 * param_str.c
 *
 *  Created on: Sep 30, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <param/param.h>
#include "param_string.h"

void param_value_str(param_t *param, char * out, int len)
{
	switch(param->type) {
#define PARAM_SWITCH_SNPRINTF(casename, strtype, strcast, name) \
	case casename: \
		snprintf(out, len, strtype, (strcast) param_get_##name(param)); \
		break;

	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT8, "%u", unsigned int, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT16, "%u", unsigned int, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT32, "%u", unsigned int, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT64, "%lu", unsigned long, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT8, "%d", unsigned int, int8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT16, "%d", unsigned int, int16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT32, "%d", unsigned int, int32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT64, "%ld", unsigned long, int64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT8, "0x%X", unsigned int, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT16, "0x%X", unsigned int, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT32, "0x%X", unsigned int, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT64, "0x%lX", unsigned long, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, "%f", float, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, "%f", double, double)

	case PARAM_TYPE_DATA: {
		char data[param->size];
		param_get_data(param, data, param->size);
		int written = snprintf(out, len, "0x");
		len -= written;
		out += written;
		for (int i = 0; i < param->size; i++) {
			written = snprintf(out, len, "%02x", data[i]);
			len -= written;
			out += written;
		}
		break;
	}

	case PARAM_TYPE_STRING:
		param_get_string(param, out, len);
		break;

	default:
		snprintf(out, len, "n/a");
		break;
#undef PARAM_SWITCH_SNPRINTF
	}
}

void param_str_to_value(param_type_e type, char * in, void * out)
{
	switch(type) {

#define PARAM_SCANF(casename, strtype, strcast, name, outcast) \
	case casename: { \
		strcast obj; \
		sscanf(in, strtype, &obj); \
		*(outcast *) out = (outcast) obj; \
		break; \
	}

	PARAM_SCANF(PARAM_TYPE_UINT8, "%u", unsigned int, uint8, uint8_t)
	PARAM_SCANF(PARAM_TYPE_UINT16, "%u", unsigned int, uint16, uint16_t)
	PARAM_SCANF(PARAM_TYPE_UINT32, "%u", unsigned int, uint32, uint32_t)
	PARAM_SCANF(PARAM_TYPE_UINT64, "%lu", unsigned long, uint64, uint64_t)
	PARAM_SCANF(PARAM_TYPE_INT8, "%d", unsigned int, int8, int8_t)
	PARAM_SCANF(PARAM_TYPE_INT16, "%d", unsigned int, int16, int16_t)
	PARAM_SCANF(PARAM_TYPE_INT32, "%d", unsigned int, int32, int32_t)
	PARAM_SCANF(PARAM_TYPE_INT64, "%ld", unsigned long, int64, int64_t)
	PARAM_SCANF(PARAM_TYPE_XINT8, "0x%X", unsigned int, uint8, uint8_t)
	PARAM_SCANF(PARAM_TYPE_XINT16, "0x%X", unsigned int, uint16, uint16_t)
	PARAM_SCANF(PARAM_TYPE_XINT32, "0x%X", unsigned int, uint32, uint32_t)
	PARAM_SCANF(PARAM_TYPE_XINT64, "0x%lX", unsigned long, uint64, uint64_t)
	PARAM_SCANF(PARAM_TYPE_FLOAT, "%f", float, float, float)
	PARAM_SCANF(PARAM_TYPE_DOUBLE, "%lf", double, double, double)

	default:
		printf("Unsupported type\r\n");
		break;
#undef PARAM_SCANF
	}
}

void param_type_str(param_t *param, char * out, int len)
{
	switch(param->type) {

#define PARAM_SWITCH_SNPRINTF(casename, name) \
	case casename: \
		snprintf(out, len, "%s", #name); \
		break;

	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT8, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT16, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT32, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT64, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT8, int8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT16, int16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT32, int32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT64, int64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT8, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT16, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT32, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT64, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, double)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DATA, data)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_STRING, string)
#undef PARAM_SWITCH_SNPRINTF
	}
}
