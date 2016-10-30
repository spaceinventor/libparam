/*
 * param_str.c
 *
 *  Created on: Sep 30, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
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
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT8, "%d", signed int, int8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT16, "%d", signed int, int16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT32, "%d", signed int, int32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT64, "%ld", signed long, int64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT8, "0x%X", unsigned int, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT16, "0x%X", unsigned int, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT32, "0x%X", unsigned int, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT64, "0x%lX", unsigned long, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, "%f", float, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, "%f", double, double)

	case PARAM_TYPE_DATA: {
		char data[param->size];
		param_get_data(param, data, param->size);
		int written;
		for (int i = 0; i < param->size; i++) {
			written = snprintf(out, len, "%02hhx", (char) data[i]);
			len -= written;
			out += written;
		}
		break;
	}

	case PARAM_TYPE_STRING:
		param_get_string(param, out, len);
		break;

#undef PARAM_SWITCH_SNPRINTF
	}
}

void param_var_str(param_type_e type, int size, void * in, char * out, int len)
{
	switch(type) {
#define PARAM_SWITCH_SNPRINTF(casename, strtype, strcast, name) \
	case casename: \
		snprintf(out, len, strtype, *(strcast *) in); \
		break;

	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT8, "%"PRIu8, uint8_t, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT16, "%"PRIu16, uint16_t, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT32, "%"PRIu32, uint32_t, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT64, "%"PRIu64, uint64_t, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT8, "%"PRId8, int8_t, int8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT16, "%"PRId16, int16_t, int16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT32, "%"PRId32, int32_t, int32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT64, "%"PRId64, int64_t, int64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT8, "0x%"PRIx8, uint8_t, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT16, "0x%"PRIx16, uint16_t, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT32, "0x%"PRIx32, uint32_t, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT64, "0x%"PRIx64, uint64_t, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, "%f", float, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, "%f", double, double)

	case PARAM_TYPE_DATA: {
		int written = snprintf(out, len, "0x");
		len -= written;
		out += written;
		for (int i = 0; i < size; i++) {
			written = snprintf(out, len, "%02x", ((char *)in)[i]);
			len -= written;
			out += written;
		}
		break;
	}

	case PARAM_TYPE_STRING:
		strncpy(out, in, size);
		break;

#undef PARAM_SWITCH_SNPRINTF
	}
}

int param_str_to_value(param_type_e type, char * in, void * out)
{
	switch(type) {

#define PARAM_SCANF(casename, strtype, outcast) \
	case casename: { \
		outcast obj; \
		sscanf(in, strtype, &obj); \
		*(outcast *) out = (outcast) obj; \
		return sizeof(outcast); \
	}
	PARAM_SCANF(PARAM_TYPE_UINT8, "%hhu", uint8_t)
	PARAM_SCANF(PARAM_TYPE_UINT16, "%"SCNu16, uint16_t)
	PARAM_SCANF(PARAM_TYPE_UINT32, "%"SCNu32, uint32_t)
	PARAM_SCANF(PARAM_TYPE_UINT64, "%"SCNu64, uint64_t)
	PARAM_SCANF(PARAM_TYPE_INT8, "%hhd", int8_t)
	PARAM_SCANF(PARAM_TYPE_INT16, "%"SCNd16, int16_t)
	PARAM_SCANF(PARAM_TYPE_INT32, "%"SCNd32, int32_t)
	PARAM_SCANF(PARAM_TYPE_INT64, "%"SCNd64, int64_t)
	PARAM_SCANF(PARAM_TYPE_XINT8, "%hhx", uint8_t)
	PARAM_SCANF(PARAM_TYPE_XINT16, "%"SCNx16, uint16_t)
	PARAM_SCANF(PARAM_TYPE_XINT32, "%"SCNx32, uint32_t)
	PARAM_SCANF(PARAM_TYPE_XINT64, "%"SCNx64, uint64_t)
	PARAM_SCANF(PARAM_TYPE_FLOAT, "%f", float)
	PARAM_SCANF(PARAM_TYPE_DOUBLE, "%lf", double)
#undef PARAM_SCANF

	case PARAM_TYPE_STRING:
		strcpy(out, in);
		return strlen(in);
			
	case PARAM_TYPE_DATA: {
		int len = strlen(in) / 2;

		unsigned int nibble(char c) {
			if (c >= '0' && c <= '9') return      c - '0';
			if (c >= 'A' && c <= 'F') return 10 + c - 'A';
			if (c >= 'a' && c <= 'f') return 10 + c - 'a';
			return -1;
		}

		for (int i = 0; i < len; i++)
			((char *) out)[i] = (nibble(in[i*2]) << 4) + nibble(in[i*2+1]);
			
		return len;
	}
	}

	return 0;
}

void param_type_str(param_type_e type, char * out, int len)
{
	switch(type) {

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
