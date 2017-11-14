/*
 * param_serializer.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_SERIALIZER_H_
#define SRC_PARAM_PARAM_SERIALIZER_H_

#include <param/param.h>
#include <csp/csp.h>
#include <mpack/mpack.h>

int param_serialize_to_mpack(param_t * param, mpack_writer_t * writer, void * value);
param_t * param_deserialize_from_mpack(mpack_reader_t * reader);
void param_deserialize_from_mpack_to_param(void * queue, param_t * param, mpack_reader_t * reader);

static inline uint16_t param_get_short_id(param_t * param, unsigned int reserved1, unsigned int reserved2) {
	uint16_t node = (param->node == 255) ? csp_get_address() : param->node;
	return (node << 11) | ((reserved1 & 1) << 10) | ((reserved2 & 1) << 2) | ((param->id) & 0x1FF);
}

static inline uint8_t param_parse_short_id_node(uint16_t short_id) {
	return (short_id >> 11) & 0x1F;
}

static inline uint16_t param_parse_short_id_paramid(uint16_t short_id) {
	return short_id & 0x1FF;
}

#endif /* SRC_PARAM_PARAM_SERIALIZER_H_ */
