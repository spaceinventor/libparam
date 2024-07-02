/*
 * vmem_crc32.h
 *
 *  Created on: Oct 26, 2023
 *      Author: thomasl
 */

#ifndef LIB_PARAM_INCLUDE_VMEM_VMEM_CRC32_H_
#define LIB_PARAM_INCLUDE_VMEM_VMEM_CRC32_H_

#include <stdint.h>
#include "vmem/vmem.h"

uint32_t vmem_calc_crc32(uint64_t addr, uint64_t len, void *buffer, uint32_t buffer_len);

#endif /* LIB_PARAM_INCLUDE_VMEM_VMEM_CRC32_H_ */
