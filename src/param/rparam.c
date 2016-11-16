/*
 * rparam.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>
#include <param/rparam.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_endian.h>

#include "param_serializer.h"
#include "param_string.h"
#include <param/rparam_list.h>

int rparam_size(rparam_t * rparam) {
	int size = param_typesize(rparam->type);
	if (size == -1) {
		size = rparam->size;
	}
	return size;
}

int rparam_get_single(rparam_t * rparam) {
	rparam_t * rparams[1] = { rparam };
	return rparam_get(rparams, 1, 0);
}

int rparam_get(rparam_t * rparams[], int count, int verbose)
{
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	uint16_t * request = packet->data16;

	int response_size = 0;

	int i;
	for (i = 0; i < count; i++) {
		if (response_size + sizeof(uint16_t) + rparam_size(rparams[i]) > PARAM_SERVER_MTU) {
			printf("Request cropped: > MTU\n");
			break;
		}

		response_size += sizeof(uint16_t) + rparam_size(rparams[i]);
		request[i] = csp_hton16(rparams[i]->id);
	}
	packet->length = sizeof(uint16_t) * i;

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

	i = 0;
	while(i < packet->length) {

		/* Get id */
		uint16_t id;
		memcpy(&id, &packet->data[i], sizeof(id));
		i += sizeof(id);
		id = csp_ntoh16(id);

		/* Search for rparam using list */
		rparam_t * rparam = rparam_list_find_id(packet->id.src, id);

		if (rparam == NULL) {
			printf("No rparam for node %u id %u\n", packet->id.src, id);
			return -1;
		}

		if (rparam->value == NULL) {
			rparam->value = calloc(rparam_size(rparam), 1);
		}

		i += param_deserialize_to_var(rparam->type, rparam->size, &packet->data[i], rparam->value);

		rparam->value_updated = csp_get_ms();
		if (rparam->setvalue_pending == 2)
			rparam->setvalue_pending = 0;

		if (verbose)
			rparam_print(rparam);

	}

	csp_buffer_free(packet);
	csp_close(conn);

	return 0;

}

#define RPARAM_GET(_type, _name) \
	_type rparam_get_##_name(rparam_t * rparam) { \
		rparam_get(&rparam, 1, 0); \
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

int rparam_set_single(rparam_t * rparam) {
	rparam_t * rparams[1] = { rparam };
	return rparam_set(rparams, 1, 0);
}

int rparam_set(rparam_t * rparams[], int count, int verbose)
{
	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	packet->length = 0;
	for (int i = 0; i < count; i++) {

		if ((rparams[i]->setvalue == NULL) || (rparams[i]->setvalue_pending != 1))
			continue;

		if (packet->length + sizeof(uint16_t) + rparam_size(rparams[i]) > PARAM_SERVER_MTU) {
			printf("Request cropped: > MTU\n");
			break;
		}

		/* Parameter id */
		uint16_t id = csp_hton16(rparams[i]->id);
		memcpy(packet->data + packet->length, &id, sizeof(uint16_t));
		packet->length += sizeof(uint16_t);

		packet->length += param_serialize_from_var(rparams[i]->type, rparams[i]->size, rparams[i]->setvalue, (char *) packet->data + packet->length);

	}

	/* If there were no parameters to be set */
	if (packet->length == 0) {
		csp_buffer_free(packet);
		return 0;
	}

	//csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, rparams[0]->node, PARAM_PORT_SET, 0, CSP_SO_NONE);
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
		//printf("No response\n");
		return -1;
	}

	//csp_hex_dump("Response", packet->data, packet->length);

	for (int i = 0; i < count; i++) {
		if ((rparams[i]->setvalue == NULL) || (rparams[i]->setvalue_pending == 0))
			continue;
		rparams[i]->setvalue_pending = 2;

		if (rparams[i]->value == NULL) {
			rparams[i]->value = calloc(rparam_size(rparams[i]), 1);
		}

		memcpy(rparams[i]->value, rparams[i]->setvalue, rparam_size(rparams[i]));

		if (verbose)
			rparam_print(rparams[i]);

	}

	csp_buffer_free(packet);
	csp_close(conn);

	return 0;
}

#define RPARAM_SET(_type, _name) \
	int rparam_set_##_name(rparam_t * rparam, _type value) { \
		*(_type *) rparam->setvalue = value; \
		rparam->setvalue_pending = 1; \
		return rparam_set(&rparam, 1, 0); \
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

	char tmpstr[22];

	printf(" %2u:%-3u", rparam->node, rparam->id);

	printf(" %-20s", rparam->name);

	/* Value */
	if (rparam->value != NULL) {

		param_var_str(rparam->type, rparam->size, rparam->value, tmpstr, 20);
		printf(" = %s", tmpstr);

		if (rparam->setvalue_pending == 2)
			printf("*");

		if (rparam->value_updated > 0)
			printf(" (%"PRIu32")", rparam->value_updated);

	}

	/* Type */
	param_type_str(rparam->type, tmpstr, 20);
	printf(" %s", tmpstr);

	if (rparam->size != 255)
		printf("[%u]", rparam->size);

	if ((rparam->setvalue != NULL) && (rparam->setvalue_pending == 1)) {
		printf(" Pending:");
		param_var_str(rparam->type, rparam->size, rparam->setvalue, tmpstr, 20);
		printf(" => %s", tmpstr);
	}

	printf("\n");

}
