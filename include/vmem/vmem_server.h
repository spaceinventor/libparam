/*
 * vmem_server.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_

#include <csp/csp.h>

#define VMEM_PORT_SERVER 13

void vmem_server_handler(csp_conn_t * conn);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_SERVER_H_ */
