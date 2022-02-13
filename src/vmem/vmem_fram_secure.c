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

/**
 * Driver API declaration from:
 * <drivers/fram.h>
 */
void fram_write_data(uint16_t addr, void *data, uint16_t len);
void fram_read_data(uint16_t addr, void *data, uint16_t len);
void fram_unlock_upper(void);
void fram_lock_upper(void);

void vmem_fram_secure_init(vmem_t * vmem)
{
	uint32_t fram_crc, ram_crc;
	vmem_fram_secure_driver_t * driver = vmem->driver;

	//printf("Vmem fram secure init: %s addr: %x, backup: %x\r\n", vmem->name, driver->fram_primary_addr, driver->fram_backup_addr);

	/* Read from primary FRAM */
	fram_read_data(driver->fram_primary_addr, driver->data, vmem->size);

	/* Check checksum (always kept in top 4 bytes of vmem) */
	memcpy(&fram_crc, driver->data + vmem->size - sizeof(uint32_t), sizeof(uint32_t));
	ram_crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	if (fram_crc == ram_crc) {
		return;
	}

	printf("%s: Primary FRAM corrupt, restoring backup\n", vmem->name);
	vmem_fram_secure_restore(vmem);

}

int vmem_fram_secure_backup(vmem_t * vmem)
{
	if (vmem->type != VMEM_TYPE_FRAM_SECURE) {
		printf("Invalid vmem type\n");
		return -1;
	}

	vmem_fram_secure_driver_t * driver = vmem->driver;
	printf("Vmem fram secure backup %s\r\n", vmem->name);

	fram_unlock_upper();

	/* Write entire RAM cache to FRAM backup */
	fram_write_data(driver->fram_backup_addr, driver->data, vmem->size - sizeof(uint32_t));

	/* Write checksum (always kept in top 4 bytes of vmem) */
	uint32_t crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	fram_write_data(driver->fram_backup_addr + vmem->size - sizeof(uint32_t), &crc, sizeof(uint32_t));

	fram_lock_upper();

	return 0;
}

int vmem_fram_secure_restore(vmem_t * vmem)
{
	if (vmem->type != VMEM_TYPE_FRAM_SECURE) {
		printf("Invalid vmem type\n");
		return -1;
	}

	uint32_t fram_crc, ram_crc;
	vmem_fram_secure_driver_t * driver = vmem->driver;

	/* Read from backup */
	fram_read_data(driver->fram_backup_addr, driver->data, vmem->size);

	/* Check checksum (always kept in top 4 bytes of vmem) */
	memcpy(&fram_crc, driver->data + vmem->size - sizeof(uint32_t), sizeof(uint32_t));
	ram_crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	if (fram_crc == ram_crc) {

		/* Write entire RAM cache to FRAM backup */
		fram_write_data(driver->fram_primary_addr, driver->data, vmem->size - sizeof(uint32_t));

		/* Write checksum (always kept in top 4 bytes of vmem) */
		fram_write_data(driver->fram_primary_addr + vmem->size - sizeof(uint32_t), &ram_crc, sizeof(uint32_t));
		return 0;
	}

	printf("%s: Backup FRAM corrupt, falling back to factory config\n", vmem->name);

	/* Reset the entire FRAM memory area */
	memset(driver->data, 0x00, vmem->size);
	fram_write_data(driver->fram_primary_addr, driver->data, vmem->size - sizeof(uint32_t));
	uint32_t crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	fram_write_data(driver->fram_primary_addr + vmem->size - sizeof(uint32_t), &crc, sizeof(uint32_t));

	/* Call fallback config */
	if (driver->fallback_fct != NULL)
		driver->fallback_fct();

	return 0;
}

void vmem_fram_secure_read(vmem_t * vmem, uint32_t addr, void * dataout, int len)
{
	vmem_fram_secure_driver_t * driver = vmem->driver;
	memcpy(dataout, driver->data + addr, len);
}

void vmem_fram_secure_write(vmem_t * vmem, uint32_t addr, void * datain, int len)
{
	vmem_fram_secure_driver_t * driver = vmem->driver;
	memcpy(driver->data + addr, datain, len);
	fram_write_data(driver->fram_primary_addr + addr, datain, len);

	/* Write checksum (always kept in top 4 bytes of vmem) */
	uint32_t crc = csp_crc32_memory(driver->data, vmem->size - sizeof(uint32_t));
	fram_write_data(driver->fram_primary_addr + vmem->size - sizeof(uint32_t), &crc, sizeof(uint32_t));
}
