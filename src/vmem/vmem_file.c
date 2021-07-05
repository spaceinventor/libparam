/*
 * vmem_file.c
 *
 *  Created on: Aug 30, 2018
 *      Author: johan
 */

#include <string.h>
#include <stdio.h>
#include <vmem/vmem.h>
#include <vmem/vmem_file.h>

void vmem_file_init(const vmem_t * vmem) {

	/* Read from file */
	FILE * stream = fopen(((vmem_file_driver_t *) vmem->driver)->filename, "r+");
	if (stream == NULL)
		return;
	int read = fread(((vmem_file_driver_t *) vmem->driver)->physaddr, 1, vmem->size, stream);
	//printf("Read %d bytes from %s\n", read, ((vmem_file_driver_t *) vmem->driver)->filename);
	fclose(stream);

}

void vmem_file_read(const vmem_t * vmem, uint32_t addr, void * dataout, int len) {
	memcpy(dataout, ((vmem_file_driver_t *) vmem->driver)->physaddr + addr, len);
}

void vmem_file_write(const vmem_t * vmem, uint32_t addr, void * datain, int len) {
	memcpy(((vmem_file_driver_t *) vmem->driver)->physaddr + addr, datain, len);

	/* Flush back to file */
	FILE * stream = fopen(((vmem_file_driver_t *) vmem->driver)->filename, "w+");
	if (stream == NULL) {
		printf("Failed to open file %s\n", ((vmem_file_driver_t *) vmem->driver)->filename);
		return;
	}
	int written = fwrite(((vmem_file_driver_t *) vmem->driver)->physaddr, 1, vmem->size, stream);
	(void) written;
	//printf("Wrote %d bytes to %s\n", written, ((vmem_file_driver_t *) vmem->driver)->filename);
	fclose(stream);
}


