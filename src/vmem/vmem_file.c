/*
 * vmem_file.c
 *
 *  Created on: Aug 30, 2018
 *      Author: johan
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <vmem/vmem.h>
#include <vmem/vmem_file.h>

void vmem_file_init(vmem_t * vmem) {
	vmem_file_driver_t *driver = (vmem_file_driver_t *) vmem->driver;
	if(driver->stream == NULL) {
		/* Open file for reading/writing, creating it if it doesn't exist */
		int fd = open(driver->filename, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
		if (fd != -1) {
			driver->stream = fdopen(fd, "r+");
		}
		if(driver->stream) {
			int read = fread(driver->physaddr, 1, vmem->size, driver->stream);
			(void) read;
		} else {
			printf("\nWARNING: vmem[%s]: permission/path issues for associated filename: \"%s\"\n", vmem->name, driver->filename);
		}
	}
}

void vmem_file_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len) {
	vmem_file_driver_t *driver = (vmem_file_driver_t *) vmem->driver;
	vmem_file_init(vmem);
	memcpy(dataout, (void*)((intptr_t)driver->physaddr + addr), len);
}

void vmem_file_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len) {
	vmem_file_driver_t *driver = (vmem_file_driver_t *) vmem->driver;
	memcpy((void *)((intptr_t)driver->physaddr + addr), datain, len);
	vmem_file_init(vmem);
	if(driver->stream ) {
		/* Flush back to file */
		int res = fseek(driver->stream, addr, SEEK_SET);
		(void)res;
		int written = fwrite(driver->physaddr + addr, len, 1, driver->stream);
		fflush(driver->stream);
		(void) written;
	}
}
