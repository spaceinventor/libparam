/*
 * vmem_file.c
 *
 *  Created on: Aug 30, 2018
 *      Author: johan
 */

#include <string.h>
#include <vmem/vmem.h>
#include <vmem/vmem_file.h>

void vmem_file_init(vmem_t * vmem) {
	vmem_file_driver_t *driver = (vmem_file_driver_t *) vmem->driver;
	/* Read from file */
	driver->stream = fopen(driver->filename, "r+");
	if (driver->stream == NULL) {
		driver->stream = fopen(driver->filename, "a");
	} else {
		int read = fread(driver->physaddr, 1, vmem->size, driver->stream);
		(void) read;
	}
}

void vmem_file_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len) {
	vmem_file_driver_t *driver = (vmem_file_driver_t *) vmem->driver;
	if(driver->stream == NULL) {
		vmem_file_init(vmem);
		if(driver->stream == NULL) {
			printf("\nWARNING: vmem[%s]: file permission issues for associated filename: \"%s\"\n", vmem->name, driver->filename);
		}
	}
	memcpy(dataout, ((vmem_file_driver_t *) vmem->driver)->physaddr + addr, len);
}

void vmem_file_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len) {
	vmem_file_driver_t *driver = (vmem_file_driver_t *) vmem->driver;
	memcpy(driver->physaddr + addr, datain, len);

	if(driver->stream == NULL) {
		vmem_file_init(vmem);
		if(driver->stream == NULL) {
			printf("\nWARNING: vmem[%s]: file permission issues for associated filename: \"%s\"\n", vmem->name, driver->filename);
		}
	}
	if(driver->stream ) {
		/* Flush back to file */
		int res = fseek(driver->stream, 0, SEEK_SET);
		(void)res;
		int written = fwrite(driver->physaddr, 1, vmem->size, driver->stream);
		fflush(driver->stream);
		(void) written;
	}
}


