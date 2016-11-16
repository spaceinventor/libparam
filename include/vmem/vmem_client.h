/*
 * vmem_client.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_

#include <stdint.h>

void vmem_download(int node, int timeout, uint32_t address, uint32_t length, char * dataout);
void vmem_upload(int node, int timeout, uint32_t address, char * datain, uint32_t length);
void vmem_client_list(int node, int timeout);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_ */
