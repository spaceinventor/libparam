#pragma once

#include <vmem/vmem.h>
#include <vmem/vmem_compress.h>

/**
 * @brief Write chunk of data to VMEM from physical memory
 *
 * @param to Virtual address to write the data to
 * @param from Physical address to read the data from
 * @param size Number of bytes to transfer
 * @return void* always NULL, no error detection possible at this time
 */
void * vmem_write_direct(vmem_t * vmem, uint64_t to, const void * from, uint32_t size);

/**
 * @brief Read chunk of data from VMEM to physical memory
 *
 * @param to Physical address to write the data to
 * @param from Virtual address to read the data from
 * @param size Number of bytes to transfer
 * @return void* always NULL, no error detection possible at this time
 */
void * vmem_read_direct(vmem_t * vmem, void * to, uint64_t from, uint32_t size);

vmem_decompress_fnc_t vmem_server_get_decompress_fnc(void);

vmem_compress_fnc_t vmem_server_get_compress_fnc(void);
