/*
 * param_serializer.h
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_SERIALIZER_H_
#define SRC_PARAM_PARAM_SERIALIZER_H_

int param_serialize_single(param_t * param, char * outbuf, int len);
int param_serialize_single_fromstr(uint16_t idx, param_type_e type, char * in, char * outbuf, int len);
void param_serialize(param_t * param[], int count, char * outbuf, int len);
void param_serialize_idx(uint16_t param_idx[], int count, char * outbuf, int len);

int param_deserialize_single(char * inbuf);

#endif /* SRC_PARAM_PARAM_SERIALIZER_H_ */
