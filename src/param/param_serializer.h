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

void param_serialize_to_mpack(param_t * param, mpack_writer_t * writer, void * value);
void param_deserialize_from_mpack(mpack_reader_t * reader);

#endif /* SRC_PARAM_PARAM_SERIALIZER_H_ */
