/*
 * param_string.h
 *
 *  Created on: Sep 30, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_PARAM_STRING_H_
#define SRC_PARAM_PARAM_STRING_H_

#include <param.h>

void param_value_str(param_t *param, char * out, int len);
void param_str_to_value(param_type_e type, char * in, void * out);
void param_type_str(param_t *param, char * out, int len);

#endif /* SRC_PARAM_PARAM_STRING_H_ */
