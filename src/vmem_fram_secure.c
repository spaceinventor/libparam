/*
 * vmem_fram_secure.c
 *
 *  Created on: Sep 28, 2016
 *      Author: johan
 */

#include <asf.h>
#include <stdio.h>
#include <stdint.h>

#include <vmem.h>

#include "vmem_fram_secure.h"

#include <fm25w256.h>


void vmem_fram_secure_init(vmem_t * vmem) {

	vmem_fram_secure_driver_t * driver = vmem->driver;

	printf("Vmem fram secure init: %s addr: %x, backup: %x\r\n", vmem->name, driver->fram_primary_addr, driver->fram_backup_addr);

	/* Read from primary FRAM */
	fm25w256_read_data(driver->fram_primary_addr, driver->data, vmem->size);

	hex_dump(vmem->name, driver->data, vmem->size);

	/* Check checksum */
	/* Read from backup */

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
	fm25w256_write_data(addr, datain, len);
	/* Write checksum */
}
