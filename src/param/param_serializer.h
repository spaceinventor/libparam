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

int param_serialize_from_param(param_t * param, char * out);
int param_serialize_from_var(param_type_e type, int size, void * in, char * out);

int param_deserialize_to_var(param_type_e type, int size, void * in, void * out);
int param_deserialize_to_param(void * in, param_t * param);

int param_deserialize_single(char * inbuf);
int param_serialize_single(param_t * param, char * outbuf, int len);

int param_deserialize_from_packet(csp_packet_t * packet);



int param_serialize_chunk_timestamp(uint32_t timestamp, uint8_t * out);
int param_deserialize_chunk_timestamp(uint32_t * timestamp, uint8_t * in);

int param_serialize_chunk_node(uint8_t node, uint8_t * out);
int param_deserialize_chunk_node(uint8_t * node, uint8_t * in);

int param_serialize_chunk_param(param_t * param, uint8_t * out);

int param_serialize_chunk_params_begin(uint8_t ** count, uint8_t * out);
int param_serialize_chunk_params_next(param_t * param, uint8_t * count, uint8_t * out);
int param_deserialize_chunk_params_begin(uint8_t * count, uint8_t * in);
int param_deserialize_chunk_params_next(uint16_t * paramid, uint8_t * in);

int param_serialize_chunk_param_and_value(param_t * params[], uint8_t count, uint8_t * out, int pending);
int param_deserialize_chunk_param_and_value(uint8_t node, uint32_t timestamp, int verbose, uint8_t * in);

void param_serialize_to_mpack(param_t * param, mpack_writer_t * writer, void * value);
void param_deserialize_from_mpack_map(mpack_reader_t * reader);

#endif /* SRC_PARAM_PARAM_SERIALIZER_H_ */
