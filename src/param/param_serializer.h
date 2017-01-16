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

int param_serialize_from_param(param_t * param, char * out);
int param_serialize_from_var(param_type_e type, int size, void * in, char * out);

int param_deserialize_to_var(param_type_e type, int size, void * in, void * out);
int param_deserialize_to_param(void * in, param_t * param);

int param_deserialize_single(char * inbuf);
int param_serialize_single(param_t * param, char * outbuf, int len);

int param_deserialize_from_packet(csp_packet_t * packet);

#endif /* SRC_PARAM_PARAM_SERIALIZER_H_ */
