#pragma once

#include <stdint.h>
#include <vmem/vmem_server.h>

typedef void (*vmem_progress_cb)(uint32_t total, uint32_t sofar);

/**
 * @brief progress callback example
 * @param total total bytes to upload
 * @param sofar bytes uploaded so far
 */
void vmem_progress(uint32_t total, uint32_t sofar);

/**
 * @brief Download content of given VMEM address of given node to given memory
 * @param node CSP address of recipient (a VMEM server must be running on this node)
 * @param timeout CSP timeout to use
 * @param address source VMEM address
 * @param datain pointer to data buffer to store downloaded data
 * @param length how many bytes should be downloaded
 * @param version VMEM version
 * @param cb optional function that will be called regularly to report progress
 * @return < 0 in case of failure (CSP_ERR_TIMEDOUT, CSP_ERR_NOBUFS), uploaded amount of bytes otherwise (may be less then the length parameter if transfer was interrupted)
 */
int vmem_download_progress(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version, int use_rdp, vmem_progress_cb cb);

/**
 * @brief Download content of given VMEM address of given node to given memory
 * @param node CSP address of recipient (a VMEM server must be running on this node)
 * @param timeout CSP timeout to use
 * @param address source VMEM address
 * @param datain pointer to data buffer to store downloaded data
 * @param length how many bytes should be downloaded
 * @param version VMEM version
 * @return < 0 in case of failure (CSP_ERR_TIMEDOUT, CSP_ERR_NOBUFS), uploaded amount of bytes otherwise (may be less then the length parameter if transfer was interrupted)
 */
int vmem_download(int node, int timeout, uint64_t address, uint32_t length, char * dataout, int version, int use_rdp);

/**
 * @brief Upload content of given memory to given VMEM address of given node
 * @param node CSP address of recipient (a VMEM server must be running on this node)
 * @param timeout CSP timeout to use
 * @param address destination VMEM address
 * @param datain pointer to data to upload
 * @param length how many bytes should be uploaded
 * @param version VMEM version
 * @param cb optional function that will be called regularly to report progress
 * @return < 0 in case of failure (CSP_ERR_TIMEDOUT, CSP_ERR_NOBUFS), uploaded amount of bytes otherwise (may be less then the length parameter if transfer was interrupted)
 */
int vmem_upload_progress(int node, int timeout, uint64_t address, char * datain, uint32_t length, int version, vmem_progress_cb progress_cb);

/**
 * @brief Upload content of given memory to given VMEM address of given node
 * @param node CSP address of recipient (a VMEM server must be running on this node)
 * @param timeout CSP timeout to use
 * @param address destination VMEM address
 * @param datain pointer to data to upload
 * @param length how many bytes should be uploaded
 * @param version VMEM version
 * @return < 0 in case of failure (CSP_ERR_TIMEDOUT, CSP_ERR_NOBUFS), uploaded amount of bytes otherwise (may be less then the length parameter if transfer was interrupted)
 */
int vmem_upload(int node, int timeout, uint64_t address, char * datain, uint32_t length, int version);

void vmem_client_list(int node, int timeout, int version);
int vmem_client_find(int node, int timeout, void * dataout, int version, char * name, int namelen);
int vmem_client_backup(int node, int vmem_id, int timeout, int backup_or_restore);
int vmem_client_calc_crc32(int node, int timeout, uint64_t address, uint32_t length, uint32_t * crc_out, int version);
int vmem_client_compress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint64_t length, int version);
int vmem_client_decompress(int node, int timeout, uint64_t src_address, uint64_t dst_address, uint64_t length, int version);

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

