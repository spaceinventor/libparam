/*
 * vmem_client.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_

#include <stdint.h>

void vmem_download(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version, int use_rdp);
void vmem_upload(int node, int timeout, uint64_t address, char * datain, uint32_t length, int version);
void vmem_client_list(int node, int timeout, int version);
int vmem_client_backup(int node, int vmem_id, int timeout, int backup_or_restore);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_ */
