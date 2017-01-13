/*
 * param_list.h
 *
 *  Created on: Jan 13, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_LIST_H_
#define LIB_PARAM_SRC_PARAM_PARAM_LIST_H_

void param_list_from_string(FILE *stream, int node_override);
void param_list_to_string(FILE * stream, int node_filter, int remote_only);

#endif /* LIB_PARAM_SRC_PARAM_PARAM_LIST_H_ */
