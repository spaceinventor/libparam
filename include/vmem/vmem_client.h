#pragma once

#include <stdint.h>
#include <vmem/vmem_server.h>

/**
 * @brief Download data from remote vmem server at vmem address
 * 
 * Use this function for downloading data from remote node.
 * 
 * @param node The remote node to upload to
 * @param timeout Timeout for download
 * @param address VMEM address to download from on remote
 * @param dataout Pointer to data buffer
 * @param length The length in bytes to download into dataout buffer
 * @param use_rdp Set flag to use RDP connection
 * @param version Protocol version to use be used. Use 2 for 64-bit VMEM address otherwise 32-bit is assumed
 * @param verbosity Set verbosity level: 0 for silent, 1 for final summary, 2 = for detailed progress
 * @return On success, the total number of bytes successfully uploaded
 * @return On error, a negative value is returned to indicate the specific error
 */
ssize_t vmem_download(uint16_t node, uint32_t timeout, uint64_t address, char * dataout, uint32_t length, int use_rdp, int version, int verbosity);

/**
 * @brief Upload data to remote vmem server at vmem address
 * 
 * Use this function for uploading data to remote node.
 * 
 * @param node The remote node to upload to
 * @param timeout Timeout for upload
 * @param address VMEM address to upload to on remote
 * @param datain Pointer to data to upload
 * @param length The length in bytes of the data passed in
 * @param use_rdp Set flag to use RDP connection
 * @param version Protocol version to use be used. Use 2 for 64-bit VMEM address otherwise 32-bit is assumed
 * @param verbosity Set verbosity level: 0 for silent, 1 for final summary, 2 = for detailed progress
 * @return On success, the total number of bytes successfully downloaded
 * @return On error, a negative value is returned to indicate the specific error
 */
ssize_t vmem_upload(uint16_t node, uint32_t timeout, uint64_t address, const char * datain, uint32_t size, int use_rdp, int version, int verbosity);
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

