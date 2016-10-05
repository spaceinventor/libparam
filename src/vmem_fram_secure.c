/*
 * vmem_fram_secure.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <asf.h>
#include <stdio.h>
#include <stdint.h>

#include <csp/csp_crc32.h>

#include <vmem.h>

#include "vmem_fram_secure.h"

#include <fm25w256.h>


void vmem_fram_secure_init(vmem_t * vmem) {

	uint32_t fram_crc, ram_crc;
	vmem_fram_secure_driver_t * driver = vmem->driver;

	printf("Vmem fram secure init: %s addr: %x, backup: %x\r\n", vmem->name, driver->fram_primary_addr, driver->fram_backup_addr);

	/* Read from primary FRAM */
	fm25w256_read_data(driver->fram_primary_addr, driver->data, vmem->size);

	hex_dump(vmem->name, driver->data, vmem->size);

	/* Check checksum (always kept in top 4 bytes of vmem) */
	memcpy(&fram_crc, driver->data + vmem->size - sizeof(uint32_t), sizeof(uint32_t));
	ram_crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	if (fram_crc == ram_crc) {
		return;
	}

	printf("Primary FRAM corrupt, restoring backup\n");

	/* Read from backup */
	fm25w256_read_data(driver->fram_backup_addr, driver->data, vmem->size);

	/* Check checksum (always kept in top 4 bytes of vmem) */
	memcpy(&fram_crc, driver->data + vmem->size - sizeof(uint32_t), sizeof(uint32_t));
	ram_crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	if (fram_crc == ram_crc) {
		return;
	}

	printf("Backup FRAM corrupt, falling back to factory config\n");

}

void vmem_fram_secure_backup(vmem_t * vmem) {
	printf("Vmem fram secure backup\r\n");
}

void vmem_fram_secure_read(vmem_t * vmem, uint16_t addr, void * dataout, int len) {
	vmem_fram_secure_driver_t * driver = vmem->driver;
	memcpy(dataout, driver->data + addr, len);
}

void vmem_fram_secure_write(vmem_t * vmem, uint16_t addr, void * datain, int len) {
	vmem_fram_secure_driver_t * driver = vmem->driver;
	memcpy(driver->data + addr, datain, len);
	fm25w256_write_data(driver->fram_primary_addr + addr, datain, len);

	/* Write checksum (always kept in top 4 bytes of vmem) */
	uint32_t primary_crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	printf("Write: Checksum is %lx\n", primary_crc);
	fm25w256_write_data(driver->fram_primary_addr + vmem->size - sizeof(uint32_t), &primary_crc, sizeof(uint32_t));

}
