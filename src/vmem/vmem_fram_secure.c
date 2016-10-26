/*
 * vmem_fram_secure.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdint.h>

#include <csp/csp_crc32.h>

#include <vmem/vmem_fram_secure.h>

#include <drivers/fm25w256.h>

void vmem_fram_secure_init(vmem_t * vmem)
{
	uint32_t fram_crc, ram_crc;
	vmem_fram_secure_driver_t * driver = vmem->driver;

	//printf("Vmem fram secure init: %s addr: %x, backup: %x\r\n", vmem->name, driver->fram_primary_addr, driver->fram_backup_addr);

	/* Read from primary FRAM */
	fm25w256_read_data(driver->fram_primary_addr, driver->data, vmem->size);

	/* Check checksum (always kept in top 4 bytes of vmem) */
	memcpy(&fram_crc, driver->data + vmem->size - sizeof(uint32_t), sizeof(uint32_t));
	ram_crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	if (fram_crc == ram_crc) {
		return;
	}

	printf("%s: Primary FRAM corrupt, restoring backup\n", vmem->name);
	vmem_fram_secure_restore(vmem);

}

void vmem_fram_secure_backup(vmem_t * vmem)
{
	vmem_fram_secure_driver_t * driver = vmem->driver;
	printf("Vmem fram secure backup %s\r\n", vmem->name);

	/* Unlock FRAM */
	fm25w256_unlock_upper();

	/* Write entire RAM cache to FRAM backup */
	fm25w256_write_data(driver->fram_backup_addr, driver->data, vmem->size - sizeof(uint32_t));

	/* Write checksum (always kept in top 4 bytes of vmem) */
	uint32_t crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	fm25w256_write_data(driver->fram_backup_addr + vmem->size - sizeof(uint32_t), &crc, sizeof(uint32_t));

	/* Lock FRAM */
	fm25w256_unlock_upper();
}

void vmem_fram_secure_restore(vmem_t * vmem)
{
	uint32_t fram_crc, ram_crc;
	vmem_fram_secure_driver_t * driver = vmem->driver;

	/* Read from backup */
	fm25w256_read_data(driver->fram_backup_addr, driver->data, vmem->size);

	/* Check checksum (always kept in top 4 bytes of vmem) */
	memcpy(&fram_crc, driver->data + vmem->size - sizeof(uint32_t), sizeof(uint32_t));
	ram_crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	if (fram_crc == ram_crc) {

		/* Write entire RAM cache to FRAM backup */
		fm25w256_write_data(driver->fram_primary_addr, driver->data, vmem->size - sizeof(uint32_t));

		/* Write checksum (always kept in top 4 bytes of vmem) */
		fm25w256_write_data(driver->fram_primary_addr + vmem->size - sizeof(uint32_t), &ram_crc, sizeof(uint32_t));
		return;
	}

	printf("%s: Backup FRAM corrupt, falling back to factory config\n", vmem->name);

	/* Reset the entire FRAM memory area */
	memset(driver->data, 0x00, vmem->size);
	fm25w256_write_data(driver->fram_primary_addr, driver->data, vmem->size - sizeof(uint32_t));
	uint32_t crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	fm25w256_write_data(driver->fram_primary_addr + vmem->size - sizeof(uint32_t), &crc, sizeof(uint32_t));

	/* Call fallback config */
	if (driver->fallback_fct != NULL)
		driver->fallback_fct();
}

void vmem_fram_secure_read(vmem_t * vmem, uint16_t addr, void * dataout, int len)
{
	vmem_fram_secure_driver_t * driver = vmem->driver;
	memcpy(dataout, driver->data + addr, len);
}

void vmem_fram_secure_write(vmem_t * vmem, uint16_t addr, void * datain, int len)
{
	vmem_fram_secure_driver_t * driver = vmem->driver;
	memcpy(driver->data + addr, datain, len);
	fm25w256_write_data(driver->fram_primary_addr + addr, datain, len);

	/* Write checksum (always kept in top 4 bytes of vmem) */
	uint32_t crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	fm25w256_write_data(driver->fram_primary_addr + vmem->size - sizeof(uint32_t), &crc, sizeof(uint32_t));
}
