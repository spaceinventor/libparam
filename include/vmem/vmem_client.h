#pragma once

#include <stdint.h>
#include <vmem/vmem_server.h>

int vmem_download(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version, int use_rdp);

/**
 * @brief Upload data to remote vmem server at vmem address
 * 
 * Use this function for uploading data to remote node.
 * 
 * @param node The remote node to upload to
 * @param timeout Timeout for upload
 * @param address VMEM address to upload to on remote
 * @param datain Pointer to data to upload
 * @param[in,out] lengthio On input, points to the desired upload size; on output, is updated with the actual size uploaded
 * @param version Protocol version to use be used. Use 2 for 64-bit VMEM address otherwise 32-bit is assumed
 * @param verbose Set verbosity level: 0 for silent, 1 for final summary, 2 = for detailed progress
 * @return int 0=success, -1=error
 */
int vmem_upload(int node, int timeout, uint64_t address, char * datain, uint32_t *lengthio, int version, int verbose);
void vmem_client_list(int node, int timeout, int version);
int vmem_client_find(int node, int timeout, void * dataout, int version, char * name, int namelen);
int vmem_client_backup(int node, int vmem_id, int timeout, int backup_or_restore);
int vmem_client_calc_crc32(int node, int timeout, uint64_t address, uint32_t length, uint32_t * crc_out, int version);

/**
 * @brief Abort any transfer that is ongoing
 * 
 * This function can be called from outside the task that is currently running a transfer
 * The transferring task will monitor the abort flag and then stop on the next coming
 * opportunity.
 * 
 * The flag is not thread safe and will thus abort all transfers.
 * The flag is reset at the next call to up or download
 * 
 */
void vmem_client_abort(void);

