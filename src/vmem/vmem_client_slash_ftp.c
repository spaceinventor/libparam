/*
 * vmem_client_slash.c
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <stdio.h>
#include <sys/stat.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include <vmem/vmem_client.h>

#include <slash/slash.h>

static int vmem_client_slash_download(struct slash *slash)
{
	int node;
	int timeout = 10000;
	uint32_t address;
	uint32_t length;
	char * file;
	char * endptr;

	if (slash->argc < 5)
		return SLASH_EUSAGE;

	node = strtoul(slash->argv[1], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	address = strtoul(slash->argv[2], &endptr, 16);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	length = strtoul(slash->argv[3], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	file = slash->argv[4];

	if (slash->argc > 5) {
		timeout = strtoul(slash->argv[5], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	printf("Download from %u addr 0x%"PRIX32" to %s with timeout %u\n", node, address, file, timeout);

	/* Allocate memory for reply */
	char * data = malloc(length);

	uint32_t time_begin = csp_get_ms();
	vmem_download(node, timeout, address, length, data);
	uint32_t time_total = csp_get_ms() - time_begin;

	/* Open file (truncate or create) */
	FILE * fd = fopen(file, "w+");
	if (fd == NULL)
		return SLASH_EINVAL;

	/* Write data */
	int written = fwrite(data, 1, length, fd);
	fclose(fd);

	printf("Downloaded %u bytes in %.03f s at %u Bps\n", written, time_total / 1000.0, (unsigned int) (written / ((float)time_total / 1000.0)) );

	return SLASH_SUCCESS;
}
slash_command(download, vmem_client_slash_download, "<node> <address> <length> <file> [timeout]", "Download from VMEM to FILE");

static int vmem_client_slash_upload(struct slash *slash)
{
	int node;
	int timeout = 2000;
	uint32_t address;
	char * file;
	char * endptr;

	if (slash->argc < 4)
		return SLASH_EUSAGE;

	file = slash->argv[1];

	node = strtoul(slash->argv[2], &endptr, 10);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	address = strtoul(slash->argv[3], &endptr, 16);
	if (*endptr != '\0')
		return SLASH_EUSAGE;

	if (slash->argc > 4) {
		timeout = strtoul(slash->argv[4], &endptr, 10);
		if (*endptr != '\0')
			return SLASH_EUSAGE;
	}

	printf("Upload from %s to node %u addr 0x%"PRIX32" with timeout %u\n", file, node, address, timeout);

	/* Open file */
	FILE * fd = fopen(file, "r");
	if (fd == NULL)
		return SLASH_EINVAL;

	/* Read size */
	struct stat file_stat;
	stat(file, &file_stat);

	/* Copy to memory */
	char * data = malloc(file_stat.st_size);
	int size = fread(data, 1, file_stat.st_size, fd);
	fclose(fd);

	//csp_hex_dump("file", data, size);

	printf("Size %u\n", size);

	vmem_upload(node, timeout, address, data, size);



	return SLASH_SUCCESS;
}
slash_command(upload, vmem_client_slash_upload, "<file> <node> <address> [timeout]", "Upload from FILE to VMEM");
