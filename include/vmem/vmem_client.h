/*
 * vmem_client.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_

#include <stdint.h>

void vmem_download(int node, int timeout, uint64_t address, char * dataout, int len);
void vmem_upload(int node, int timeout, uint64_t address, char * datain, int len);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_ */
