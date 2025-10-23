#pragma once
#include <stdint.h>

typedef enum {
	VMEM_SERVER_SUCESS,
	VMEM_SERVER_ENOSYS,
} vmem_response;

/**
 * @brief User-definable decompression function.
 * @param[out] dst_addr Destination address.
 * @param[out] dst_len Pointer to store the final decompressed size.
 * @param[in] src_addr Source address of the compressed data.
 * @param[in] src_len Length of the compressed data.
 * @return 0 on success, negative on error.
 */
typedef int (*vmem_decompress_fnc_t)(uint64_t dst_addr, uint64_t *dst_len, uint64_t src_addr, uint64_t src_len);

/**
 * @brief User-definable compression function.
 */
typedef int (*vmem_compress_fnc_t)(uint64_t dst_addr, uint64_t *dst_len, uint64_t src_addr, uint64_t src_len);

/**
 * @brief Register a decompression function for the VMEM server.
 * @param[in] fnc The function to handle decompression requests.
 */
void vmem_server_set_decompress_fnc(vmem_decompress_fnc_t fnc);

/**
 * @brief Register a compression function for the VMEM server.
 * @param[in] fnc The function to handle compression requests.
 */
void vmem_server_set_compress_fnc(vmem_compress_fnc_t fnc);


vmem_decompress_fnc_t vmem_server_get_decompress_fnc(void);

vmem_compress_fnc_t vmem_server_get_compress_fnc(void);
