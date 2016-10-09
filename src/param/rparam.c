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
#include <slash/slash.h>
#include <csp/csp.h>
#include <csp/csp_endian.h>
#include "param_serializer.h"

int rparam_get(rparam_t * rparam, void * out)
{
	//printf("Rparam get %s\n", rparam->name);

	csp_packet_t * packet = csp_buffer_get(256);
	if (packet == NULL)
		return -1;

	uint16_t * request = (uint16_t *) packet->data;
	request[0] = csp_hton16(rparam->idx);
	packet->length = sizeof(request[0]);

	//csp_hex_dump("request", packet->data, packet->length);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, rparam->node, PARAM_PORT_GET, 0, CSP_SO_NONE);
	if (conn == NULL) {
		csp_buffer_free(packet);
		return -1;
	}

	if (!csp_send(conn, packet, 0)) {
		csp_close(conn);
		csp_buffer_free(packet);
		return -1;
	}

	packet = csp_read(conn, rparam->timeout);
	if (packet == NULL) {
		csp_close(conn);
		return -1;
	}

	//csp_hex_dump("Response", packet->data, packet->length);
	csp_buffer_free(packet);
	csp_close(conn);

	param_deserialize_to_var(rparam->type, packet->data + 2, out);

	return 0;

}

#define RPARAM_GET(_type, _name) \
	_type rparam_get_##_name(rparam_t * rparam) { \
		_type obj; \
		rparam_get(rparam, &obj); \
		return obj; \
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


static int rparam_test(struct slash *slash)
{
	rparam_t rparam;
	rparam.idx = 1;
	rparam.name = "flt";
	rparam.node = 1;
	rparam.type = PARAM_TYPE_FLOAT;
	rparam.timeout = 1000;

	float flt = rparam_get_float(&rparam);

	printf("Got %f\n", flt);

	return SLASH_SUCCESS;
}
slash_command(test, rparam_test, NULL, "List parameters");


