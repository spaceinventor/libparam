/*
 * rparam.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <inttypes.h>
#include <param/rparam.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_clock.h>
#include <csp/csp_endian.h>

#include "param_serializer.h"
#include "param_string.h"
#include <param/rparam_list.h>

/**
 * Responses from rparam get are stateless (ie. contains both key and values)
 * This function does not free the packet
 */
static int rparam_deserialize_packet(csp_packet_t * packet, int verbose) {

	int i = 0;
	while(i < packet->length) {

		/* Get index */
		uint16_t idx;
		memcpy(&idx, &packet->data[i], sizeof(idx));
		i += sizeof(idx);
		idx = csp_ntoh16(idx);

		/* Search for rparam using list */
		rparam_t * rparam = rparam_list_find_idx(packet->id.src, idx);

		if (rparam == NULL) {
			printf("No rparam for node %u idx %u\n", packet->id.src, idx);
			return -1;
		}

		if (rparam->value == NULL) {

			/* Allocate storage for parameter data */
			int valuesize = param_typesize(rparam->type);
			if (valuesize == -1) {
				valuesize = rparam->size;
			}

			rparam->value = calloc(valuesize, 1);

		}

		i += param_deserialize_to_var(rparam->type, rparam->size, &packet->data[i], rparam->value);

		csp_timestamp_t now;
		clock_get_time(&now);
		rparam->value_updated = now.tv_sec;

		if (verbose)
			rparam_print(rparam);

	}

	return 0;

}

int rparam_get(rparam_t * rparams[], int count)
{
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	uint16_t * request = packet->data16;

	for (int i = 0; i < count; i++) {
		request[i] = csp_hton16(rparams[i]->idx);
	}
	packet->length = sizeof(uint16_t) * count;

	//csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, rparams[0]->node, PARAM_PORT_GET, 0, CSP_SO_NONE);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return -1;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -1;
	}

	packet = csp_read(conn, rparams[0]->timeout);
	if (packet == NULL) {
		csp_close(conn);
		return -1;
	}

	//csp_hex_dump("Response", packet->data, packet->length);

	rparam_deserialize_packet(packet, 1);

	csp_buffer_free(packet);
	csp_close(conn);

	return 0;

}

#define RPARAM_GET(_type, _name) \
	_type rparam_get_##_name(rparam_t * rparam) { \
		rparam_get(&rparam, 1); \
		return *(_type *) rparam->value; \
	} \

RPARAM_GET(uint8_t, uint8)
RPARAM_GET(uint16_t, uint16)
RPARAM_GET(uint32_t, uint32)
RPARAM_GET(uint64_t, uint64)
RPARAM_GET(int8_t, int8)
RPARAM_GET(int16_t, int16)
RPARAM_GET(int32_t, int32)
RPARAM_GET(int64_t, int64)
RPARAM_GET(float, float)
RPARAM_GET(double, double)

int rparam_set(rparam_t * rparams[], int count)
{
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	packet->length = 0;
	for (int i = 0; i < count; i++) {

		if ((rparams[i]->setvalue == NULL) || (rparams[i]->setvalue_pending == 0))
			continue;

		/* Parameter id */
		uint16_t idx = csp_hton16(rparams[i]->idx);
		memcpy(packet->data, &idx, sizeof(uint16_t));
		packet->length += sizeof(uint16_t);

		packet->length += param_serialize_from_var(rparams[i]->type, rparams[i]->size, rparams[i]->setvalue, (char *) packet->data + packet->length);

	}

	csp_hex_dump("request", packet->data, packet->length);

	if (csp_sendto(CSP_PRIO_HIGH, rparams[0]->node, PARAM_PORT_SET, 0, CSP_SO_NONE, packet, 0) != CSP_ERR_NONE) {
		csp_buffer_free(packet);
		return -1;
	}

	for (int i = 0; i < count; i++) {
		if ((rparams[i]->setvalue == NULL) || (rparams[i]->setvalue_pending == 0))
			continue;
		rparams[i]->setvalue_pending = 0;
	}


	return 0;
}

#define RPARAM_SET(_type, _name) \
	void rparam_set_##_name(rparam_t * rparam, _type value) { \
		*(_type *) rparam->setvalue = value; \
		rparam->setvalue_pending = 1; \
		rparam_set(&rparam, 1); \
	} \

RPARAM_SET(uint8_t, uint8)
RPARAM_SET(uint16_t, uint16)
RPARAM_SET(uint32_t, uint32)
RPARAM_SET(uint64_t, uint64)
RPARAM_SET(int8_t, int8)
RPARAM_SET(int16_t, int16)
RPARAM_SET(int32_t, int32)
RPARAM_SET(int64_t, int64)
RPARAM_SET(float, float)
RPARAM_SET(double, double)

void rparam_print(rparam_t * rparam) {

	char tmpstr[21];

	printf(" %u ", rparam->node);

	printf(" %-20s", rparam->name);

	/* Value */
	if ((rparam->value != NULL) && (rparam->value_updated > 0)) {
		param_var_str(rparam->type, rparam->size, rparam->value, tmpstr, 20);
		printf(" = %s", tmpstr);
		printf(" (%lu)", rparam->value_updated);
	}

	/* Type */
	param_type_str(rparam->type, tmpstr, 20);
	printf(" %s", tmpstr);

	if (rparam->size != 255)
		printf("[%u]", rparam->size);

	if ((rparam->setvalue != NULL) && (rparam->setvalue_pending)) {
		printf(" Pending:");
		param_var_str(rparam->type, rparam->size, rparam->setvalue, tmpstr, 20);
		printf(" => %s", tmpstr);
	}

	printf("\n");

}

#if 0
static int rparam_test(struct slash *slash)
{
	rparam_t rparam;
	rparam.idx = 1;
	strcpy(rparam.name, "flt");
	rparam.node = 1;
	rparam.type = PARAM_TYPE_FLOAT;
	rparam.timeout = 1000;

	float flt = 2.34;
	rparam_set_float(&rparam, flt);

	flt = rparam_get_float(&rparam);

	printf("Got %f\n", flt);

	return SLASH_SUCCESS;
}
slash_command(test, rparam_test, NULL, "List parameters");

#endif
