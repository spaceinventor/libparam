/*
 * param_str.c
 *
 *  Created on: Sep 30, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>
#include <csp/arch/csp_time.h>
#include <param/param.h>
#include <param/param_list.h>

#include "param_string.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

void param_value_str(param_t *param, unsigned int i, char * out, int len)
{
	switch(param->type) {
#define PARAM_SWITCH_SNPRINTF(casename, strtype, strcast, name) \
	case casename: \
		snprintf(out, len, strtype, (strcast) param_get_##name##_array(param, i)); \
		break;

	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT8, "%u", unsigned char, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT16, "%u", unsigned short, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT32, "%u", unsigned int, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT64, "%lu", unsigned long, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT8, "%d", signed char, int8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT16, "%d", signed short, int16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT32, "%d", signed int, int32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT64, "%ld", signed long, int64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT8, "0x%X", unsigned char, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT16, "0x%X", unsigned short, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT32, "0x%X", unsigned int, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT64, "0x%lX", unsigned long, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, "%.04f", float, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, "%f", double, double)

	case PARAM_TYPE_DATA: {
		char data[param->array_size];
		param_get_data(param, data, param->array_size);
		int written;
		for (int i = 0; i < param->array_size && len >= 2; i++) {
			written = snprintf(out, len, "%02X", (unsigned char) data[i]);
			len -= written;
			out += written;
		}
		break;
	}

	case PARAM_TYPE_STRING:
		param_get_string(param, out, MIN(param->array_size, len));
		break;

#undef PARAM_SWITCH_SNPRINTF
	}
}

int param_str_to_value(param_type_e type, char * in, void * out)
{
	switch(type) {

#define PARAM_SCANF(casename, strtype, strcast, name, outcast) \
	case casename: { \
		strcast obj; \
		sscanf(in, strtype, &obj); \
		*(outcast *) out = (outcast) obj; \
		return sizeof(outcast); \
	}

	PARAM_SCANF(PARAM_TYPE_UINT8, "%u", unsigned int, uint8, uint8_t)
	PARAM_SCANF(PARAM_TYPE_UINT16, "%u", unsigned int, uint16, uint16_t)
	PARAM_SCANF(PARAM_TYPE_UINT32, "%u", unsigned int, uint32, uint32_t)
	PARAM_SCANF(PARAM_TYPE_UINT64, "%lu", unsigned long, uint64, uint64_t)
	PARAM_SCANF(PARAM_TYPE_INT8, "%d", int, int8, int8_t)
	PARAM_SCANF(PARAM_TYPE_INT16, "%d", int, int16, int16_t)
	PARAM_SCANF(PARAM_TYPE_INT32, "%d", int, int32, int32_t)
	PARAM_SCANF(PARAM_TYPE_INT64, "%ld", long, int64, int64_t)
	PARAM_SCANF(PARAM_TYPE_XINT8, "0x%X", unsigned int, uint8, uint8_t)
	PARAM_SCANF(PARAM_TYPE_XINT16, "0x%X", unsigned int, uint16, uint16_t)
	PARAM_SCANF(PARAM_TYPE_XINT32, "0x%X", unsigned int, uint32, uint32_t)
	PARAM_SCANF(PARAM_TYPE_XINT64, "0x%lX", unsigned long, uint64, uint64_t)
	PARAM_SCANF(PARAM_TYPE_FLOAT, "%f", float, float, float)
	PARAM_SCANF(PARAM_TYPE_DOUBLE, "%lf", double, double, double)
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
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_STRING, string)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DATA, data)
#undef PARAM_SWITCH_SNPRINTF
	}
}

static void param_print_value(param_t * param, int offset) {

	if (param == NULL) {
		return;
	}

	printf(" = ");

	int count = (param->array_size > 0) ? param->array_size : 1;

	/* Treat data and strings as single parameters */
	if (param->type == PARAM_TYPE_DATA || param->type == PARAM_TYPE_STRING)
		count = 1;

	/* If offset is set, adjust count to only display one value */
	if (offset >= 0) {
		count = 1;
	}

	/* If offset is unset, start at zero and display all values */
	if (offset < 0) {
		offset = 0;
	}

	if (count > 1)
		printf("[");

	for(int i = offset; i < offset + count; i++) {
		char value_str[40];
		param_value_str(param, i, value_str, 40);
		if (param->type == PARAM_TYPE_STRING) {
			printf("\"%s\"", value_str);
		} else {
			printf("%s", value_str);
		}
		if (i + 1 < count)
			printf(" ");
	}

	if (count > 1)
		printf("]");

}

void param_print(param_t * param, int offset, int nodes[], int nodes_count, int verbose)
{
	if (param == NULL)
		return;

	/* Node/ID */
	if (param->node != PARAM_LIST_LOCAL) {
		printf(" %3u:%-2u", param->id, param->node);
	} else {
		printf(" %3u:L ", param->id);
	}

	/* Name */
	printf(" %-20s", param->name);

	/* Value table */
	if (nodes_count > 0 && nodes != NULL) {
		for(int i = 0; i < nodes_count; i++) {
			param_t * specific_param = param_list_find_id(nodes[i], param->id);
			param_print_value(specific_param, offset);
		}

	/* Single value */
	} else {
		param_print_value(param, offset);
	}

	/* Unit */
	if (verbose >= 1) {
		if (param->unit != NULL && strlen(param->unit))
			printf(" %s", param->unit);
	}

	if (verbose >= 2) {
		/* Type */
		char type_str[11] = {};
		param_type_str(param->type, type_str, 10);
		printf(" %s", type_str);

		/* Size */
		if (param->array_size > 0)
			printf("[%u]", param->array_size);

	}

	printf("\n");

}
