/*
 * param_serializer.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_SERIALIZER_H_
#define SRC_PARAM_PARAM_SERIALIZER_H_

int param_serialize_from_str(param_type_e type, char * str, void * out);
int param_serialize_from_param(param_t * param, char * out);
int param_serialize_from_var(param_type_e type, int size, void * in, char * out);

int param_deserialize_to_var(param_type_e type, int size, void * in, void * out);
int param_deserialize_to_param(void * in, param_t * param);

int param_deserialize_single(char * inbuf);
int param_serialize_single(param_t * param, char * outbuf, int len);
int param_serialize_single_fromstr(uint16_t idx, param_type_e type, char * in, char * outbuf, int len);
int param_serialize(param_t * param[], int count, char * outbuf, int len);
int param_serialize_idx(uint16_t param_idx[], int count, char * outbuf, int len);


#endif /* SRC_PARAM_PARAM_SERIALIZER_H_ */
