/*
 * vmem_crc32.c
 *
 *  Created on: Oct 26, 2023
 *      Author: thomasl
 */

#include <stdint.h>
#include <string.h>

#include "vmem/vmem.h"
#include "vmem/vmem_crc32.h"
#include "csp/csp_crc32.h"

uint32_t vmem_calc_crc32(uint64_t addr, uint64_t len, void * buffer, uint32_t buffer_len) {

	csp_crc32_t crc_obj;
	uint64_t count = 0;
	uint32_t chunk_len;

	csp_crc32_init(&crc_obj);

	/* We already have a packet from the request, so use that */
	while (count < len) {
		/* Get the minimum size of each chunk */
		chunk_len = (buffer_len > (len - count) ? (len - count) : buffer_len);

		/* Grab the data from the vmem area */
		vmem_read(buffer, addr + count, chunk_len);

		/* Update CRC32 calculation */
		csp_crc32_update(&crc_obj, buffer, chunk_len);

		/* Increment */
		count += chunk_len;
	}

	/* Finalize the CRC32 calculation */
	uint32_t crc = csp_crc32_final(&crc_obj);

	return crc;

}
