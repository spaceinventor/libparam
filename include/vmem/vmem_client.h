/*
 * vmem_client.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_

#include <stdint.h>
#include <vmem/vmem_server.h>

unsigned int vmem_download(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version, int use_rdp);
void vmem_upload(int node, int timeout, uint64_t address, char * datain, uint32_t length, int version);
void vmem_client_list(int node, int timeout, int version);
vmem_list_t vmem_client_find(int node, int timeout, int version, char * name, int namelen);
int vmem_client_backup(int node, int vmem_id, int timeout, int backup_or_restore);
int vmem_client_calc_crc32(int node, int timeout, uint64_t address, uint32_t length, uint32_t * crc_out, int version);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_CLIENT_H_ */
