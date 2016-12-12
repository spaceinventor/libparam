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
#include <param/param.h>
#include "param_string.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

void param_value_str(param_t *param, char * out, int len)
{
	switch(param->type) {
#define PARAM_SWITCH_SNPRINTF(casename, strtype, strcast, name) \
	case casename: \
		snprintf(out, len, strtype, (strcast) param_get_##name(param)); \
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
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, "%f", float, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, "%f", double, double)

	case PARAM_TYPE_DATA: {
		char data[param->size];
		param_get_data(param, data, param->size);
		int written;
		for (int i = 0; i < param->size && len >= 2; i++) {
			written = snprintf(out, len, "%02X", (unsigned char) data[i]);
			len -= written;
			out += written;
		}
		break;
	}

	case PARAM_TYPE_STRING:
		param_get_string(param, out, MIN(param->size, len));
		break;

	case PARAM_TYPE_VECTOR3: {
		param_type_vector3 vect;
		param_get_data(param, &vect, sizeof(param_type_vector3));
		snprintf(out, len, "[%.2f %.2f %.2f] |%.2f|", vect.x, vect.y, vect.z, sqrtf((powf(vect.x, 2) + powf(vect.y, 2) + powf(vect.z, 2))));
		break;
	}

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
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, "%f", float, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, "%f", double, double)

	case PARAM_TYPE_DATA: {
		int written;
		for (int i = 0; i < size && len >= 2; i++) {
			written = snprintf(out, len, "%02X", ((unsigned char *)in)[i]);
			len -= written;
			out += written;
		}
		break;
	}

	case PARAM_TYPE_STRING:
		strncpy(out, in, MIN(size, len));
		break;

	case PARAM_TYPE_VECTOR3: {
		param_type_vector3 *vect = in;
		snprintf(out, len, "[%.2f %.2f %.2f] |%.2f|", vect->x, vect->y, vect->z, sqrtf((powf(vect->x, 2) + powf(vect->y, 2) + powf(vect->z, 2))));
		break;
	}

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

	case PARAM_TYPE_VECTOR3: {
		param_type_vector3 *vect = out;
		sscanf(in, "%f %f %f", &vect->x, &vect->y, &vect->z);
		return sizeof(param_type_vector3);
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
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_VECTOR3, vect3)
#undef PARAM_SWITCH_SNPRINTF
	}
}

void param_print(param_t * param)
{
	if (param == NULL)
		return;

	/* Node/ID */
	if (param->paramtype == 1) {
		printf(" %2u:%-3u", param->node, param->id);
	} else {
		printf("   :%-3u", param->id);
	}

	/* Name */
	printf(" %-20s", param->name);

	/* value */
	char value_str[41] = {};
	if (param->paramtype == 0) {

		param_value_str(param, value_str, 40);
		printf(" = %s", value_str);

	} else if (param->paramtype == 1) {

		if ((param->value_get != NULL) && (param->value_updated > 0)) {
			param_var_str(param->type, param->size, param->value_get, value_str, 40);
			printf(" = %s", value_str);

			if (param->value_pending == 2)
				printf("*");

		}
	}

	/* Unit */
	if (param->unit != NULL)
		printf(" %s", param->unit);

	/* Type */
	char type_str[11] = {};
	param_type_str(param->type, type_str, 10);
	printf(" %s", type_str);

	if (param->size > 0)
		printf("[%u]", param->size);

	if (param->paramtype == 1) {
		if ((param->value_set != NULL) && (param->value_pending == 1)) {
			printf(" Pending:");
			param_var_str(param->type, param->size, param->value_set, value_str, 40);
			printf(" => %s", value_str);
		}
	}

	printf("\n");

}

#if 0

void rparam_print(param_t * rparam) {

	char tmpstr[41] = {};

	printf(" %2u:%-3u", rparam->node, rparam->id);

	printf(" %-20s", rparam->name);

	/* Value */
	if (rparam->value_get != NULL) {

		param_var_str(rparam->type, rparam->size, rparam->value_get, tmpstr, 40);
		printf(" = %s", tmpstr);

		if (rparam->value_pending == 2)
			printf("*");

		if (rparam->value_updated > 0)
			printf(" (%"PRIu32")", rparam->value_updated);

	}

	/* Type */
	param_type_str(rparam->type, tmpstr, 10);
	printf(" %s", tmpstr);

	if (rparam->size != 255)
		printf("[%u]", rparam->size);

	if ((rparam->value_set != NULL) && (rparam->value_pending == 1)) {
		printf(" Pending:");
		param_var_str(rparam->type, rparam->size, rparam->value_set, tmpstr, 40);
		printf(" => %s", tmpstr);
	}

	printf("\n");

}

#endif
