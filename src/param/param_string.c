/*
 * param_str.c
 *
 *  Created on: Sep 30, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <csp/arch/csp_time.h>
#include <param/param.h>
#include <param/param_list.h>

#include "param_string.h"

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef SCNd8
#define __SCN8(x) __INT8 __STRINGIFY(x)
#define SCNd8       __SCN8(d)
#define SCNu8       __SCN8(u)
#define SCNx8       __SCN8(x)
#endif

void param_value_str(param_t *param, unsigned int i, char * out, int len)
{
	switch (param->type) {
#define PARAM_SWITCH_SNPRINTF(casename, strtype, strcast, name) \
	case casename: \
		snprintf(out, len, strtype, (strcast) param_get_##name##_array(param, i)); \
		break;

	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT8, "%"PRIu8, uint8_t, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT16, "%"PRIu16, uint16_t, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT32, "%"PRIu32, uint32_t, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT64, "%"PRIu64, uint64_t, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT8, "%"PRId8, int8_t, int8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT16, "%"PRId16, int16_t, int16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT32, "%"PRId32, int32_t, int32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT64, "%"PRId64, int64_t, int64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT8, "0x%"PRIX8, uint8_t, uint8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT16, "0x%"PRIX16, uint16_t, uint16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT32, "0x%"PRIX32, uint32_t, uint32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT64, "0x%"PRIX64, uint64_t, uint64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT, "%f", float, float)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, "%f", double, double)

	case PARAM_TYPE_DATA: {

		/* Prepend data with 0x */
		snprintf(out, len, "0x");
		len -= 2;
		out += 2;

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

int param_str_to_value(param_type_e type, char *in, void *out) {
	switch (type) {

#define PARAM_SCANF(casename, strtype, cast, name) \
	case casename: { \
		cast obj; \
		sscanf(in, strtype, &obj); \
		*(cast *) out = (cast) obj; \
		return sizeof(cast); \
	}

	PARAM_SCANF(PARAM_TYPE_UINT8, "%"SCNu8, uint8_t, uint8)
	PARAM_SCANF(PARAM_TYPE_UINT16, "%"SCNu16, uint16_t, uint16)
	PARAM_SCANF(PARAM_TYPE_UINT32, "%"SCNu32, uint32_t, uint32)
	PARAM_SCANF(PARAM_TYPE_UINT64, "%"SCNu64, uint64_t, uint64)
	PARAM_SCANF(PARAM_TYPE_INT8, "%"SCNd8, int8_t, int8)
	PARAM_SCANF(PARAM_TYPE_INT16, "%"SCNd16, int16_t, int16)
	PARAM_SCANF(PARAM_TYPE_INT32, "%"SCNd32, int32_t, int32)
	PARAM_SCANF(PARAM_TYPE_INT64, "%"SCNd64, int64_t, int64)
	PARAM_SCANF(PARAM_TYPE_XINT8, "0x%"SCNx8, uint8_t, uint8)
	PARAM_SCANF(PARAM_TYPE_XINT16, "0x%"SCNx16, uint16_t, uint16)
	PARAM_SCANF(PARAM_TYPE_XINT32, "0x%"SCNx32, uint32_t, uint32)
	PARAM_SCANF(PARAM_TYPE_XINT64, "0x%"SCNx64, uint64_t, uint64)
	PARAM_SCANF(PARAM_TYPE_FLOAT, "%f", float, float)
	PARAM_SCANF(PARAM_TYPE_DOUBLE, "%lf", double, double)
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

	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT8,  u08)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT16, u16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT32, u32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_UINT64, u64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT8,   i08)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT16,  i16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT32,  i32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_INT64,  i64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT8,  ut8)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT16, u16)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT32, u32)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_XINT64, u64)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_FLOAT,  flt)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DOUBLE, dbl)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_STRING, str)
	PARAM_SWITCH_SNPRINTF(PARAM_TYPE_DATA,   dat)
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
		char value_str[128] = {};
		param_value_str(param, i, value_str, 128);
		if (param->type == PARAM_TYPE_STRING) {
			printf("%s", value_str);
			int remain = 20 - strlen(value_str);
			while(remain--) {
				printf(" ");
			}
		} else {
			if (count <= 1) {
			    printf("%-20s", value_str);
			} else {
				printf("%s", value_str);
			}
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

	printf("%s", param_mask_color(param));

	/* Node/ID */
	if (verbose >= 1) {
		printf(" %3u:%-2u", param->id, param->node);
	}

	/* Name */
	printf("  %-20s", param->name);

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
		if (param->array_size > 1) {
			printf("[%u]", param->array_size);
		}

		printf("\t");
		
		if (param->mask > 0) {
			unsigned int mask = param->mask;

			printf(" ");

			if (mask & PM_READONLY) {
				mask &= ~ PM_READONLY;
				printf("r");
			}

			if (mask & PM_REMOTE) {
				mask &= ~ PM_REMOTE;
				printf("R");
			}

			if (mask & PM_CONF) {
				mask &= ~ PM_CONF;
				printf("c");
			}

			if (mask & PM_TELEM) {
				mask &= ~ PM_TELEM;
				printf("t");
			}

			if (mask & PM_HWREG) {
				mask &= ~ PM_HWREG;
				printf("h");
			}

			if (mask & PM_ERRCNT) {
				mask &= ~ PM_ERRCNT;
				printf("e");
			}

			if (mask & PM_SYSINFO) {
				mask &= ~ PM_SYSINFO;
				printf("i");
			}

			if (mask & PM_SYSCONF) {
				mask &= ~ PM_SYSCONF;
				printf("C");
			}

			if (mask & PM_WDT) {
				mask &= ~ PM_WDT;
				printf("w");
			}

			if (mask & PM_DEBUG) {
				mask &= ~ PM_DEBUG;
				printf("d");
			}

			if (mask & PM_ATOMIC_WRITE) {
				mask &= ~ PM_ATOMIC_WRITE;
				printf("o");
			}

			if (mask)
				printf("+%x", mask);

		}



	}

	if ((verbose >= 3) && (param->docstr != NULL)) {
		printf("\t\t%s", param->docstr);
	}

	printf("%s", param_mask_color_off());

	printf("\n");

}

uint32_t param_maskstr_to_mask(char * str) {

	if (str == NULL)
		return 0xFFFFFFFF;

	/* Try to parse as hex number */
	if (str[0] == '0' && (str[1] == 'X' || str[1] == 'x')) {
		return (int) strtol(str, NULL, 16);
	}

	uint32_t mask = 0;

	/* Otherwise, parse as letters */
	if (strchr(str, 'r')) mask |= PM_READONLY;
	if (strchr(str, 'R')) mask |= PM_REMOTE;
	if (strchr(str, 'c')) mask |= PM_CONF;
	if (strchr(str, 't')) mask |= PM_TELEM;
	if (strchr(str, 'h')) mask |= PM_HWREG;
	if (strchr(str, 'e')) mask |= PM_ERRCNT;
	if (strchr(str, 'i')) mask |= PM_SYSINFO;
	if (strchr(str, 'C')) mask |= PM_SYSCONF;
	if (strchr(str, 'w')) mask |= PM_WDT;
	if (strchr(str, 'd')) mask |= PM_DEBUG;
	if (strchr(str, 'o')) mask |= PM_ATOMIC_WRITE;
	if (strchr(str, 'A')) mask |= 0xFFFFFFFF;

	return mask;

}

char * param_mask_color(param_t *param) {
    unsigned int mask = param->mask;

    if (mask & PM_CONF) {
        return "\033[33m";
    }

    if (mask & PM_TELEM) {
        return "\033[32m";
    }

    if (mask & PM_HWREG) {
        return "\033[31m";
    }

    if (mask & PM_DEBUG) {
        return "\033[34m";
    }

	return "\033[0m";

}

char * param_mask_color_off(void) {
	return "\033[0m";
}