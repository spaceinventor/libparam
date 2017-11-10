/*
 * param_client.h
 *
 *  Created on: Dec 14, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_PARAM_PARAM_CLIENT_H_
#define LIB_PARAM_INCLUDE_PARAM_PARAM_CLIENT_H_

#include <csp/csp.h>
#include <param/param.h>
#include <param/param_queue.h>

int param_push_single(param_t *param, void *value, int verbose, int host, int timeout);
int param_push_queue(param_queue_t *queue, int verbose, int host, int timeout);
void param_pull_response(csp_packet_t * packet, int verbose);
int param_pull_single(param_t * param, int verbose, int host, int timeout);
int param_pull_queue(param_queue_t *queue, int verbose, int host, int timeout);

#endif /* LIB_PARAM_INCLUDE_PARAM_PARAM_CLIENT_H_ */
