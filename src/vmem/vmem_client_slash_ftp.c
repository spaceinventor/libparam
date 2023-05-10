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
#include <slash/optparse.h>
#include <slash/dflopt.h>

static int vmem_client_slash_download(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 1;

    optparse_t * parser = optparse_new("download", "<address> <length base10 or base16> <file>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");

	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;
	unsigned int use_rdp = 1;
	optparse_add_unsigned(parser, 'w', "window", "NUM", 0, &window, "rdp window (default = 3 packets)");
	optparse_add_unsigned(parser, 'c', "conn_timeout", "NUM", 0, &conn_timeout, "rdp connection timeout in ms (default = 10 seconds)");
	optparse_add_unsigned(parser, 'p', "packet_timeout", "NUM", 0, &packet_timeout, "rdp packet timeout in ms (default = 5 seconds)");
	optparse_add_unsigned(parser, 'k', "ack_timeout", "NUM", 0, &ack_timeout, "rdp max acknowledgement interval in ms (default = 2 seconds)");
	optparse_add_unsigned(parser, 'a', "ack_count", "NUM", 0, &ack_count, "rdp ack for each (default = 2 packets)");
	optparse_add_unsigned(parser, 'r', "use_rdp", "NUM", 0, &use_rdp, "rdp ack for each (default = 1)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
		return SLASH_EUSAGE;
	}

	/* Expect length */
	if (++argi >= slash->argc) {
		printf("missing length\n");
		return SLASH_EINVAL;
	}

	uint32_t length = strtoul(slash->argv[argi], &endptr, 10);
	if (*endptr != '\0') {
		length = strtoul(slash->argv[argi], &endptr, 16);
		if (*endptr != '\0') {
			printf("Failed to parse length in base 10 or base 16\n");
			return SLASH_EUSAGE;
		}
	}

	/* Expect filename */
	if (++argi >= slash->argc) {
		printf("missing filename\n");
		return SLASH_EINVAL;
	}

	char * file;
	file = slash->argv[argi];

	printf("Download from %u addr 0x%"PRIX64" to %s with timeout %u version %u\n", node, address, file, timeout, version);

	/* Allocate memory for reply */
	char * data = malloc(length);

	vmem_download(node, timeout, address, length, data, version, use_rdp);

	/* Open file (truncate or create) */
	FILE * fd = fopen(file, "w+");
	if (fd == NULL) {
		free(data);
		return SLASH_EINVAL;
	}

	/* Write data */
	int written = fwrite(data, 1, length, fd);
	fclose(fd);
	free(data);

	printf("wrote %u bytes to %s\n", written, file);

	return SLASH_SUCCESS;
}
slash_command(download, vmem_client_slash_download, "<address> <length> <file>", "Download from VMEM to FILE");

static int vmem_client_slash_upload(struct slash *slash)
{

	unsigned int node = slash_dfl_node;
    unsigned int timeout = slash_dfl_timeout;
    unsigned int version = 1;

    optparse_t * parser = optparse_new("upload", "<file> <address>");
    optparse_add_help(parser);
    optparse_add_unsigned(parser, 'n', "node", "NUM", 0, &node, "node (default = <env>)");
    optparse_add_unsigned(parser, 't', "timeout", "NUM", 0, &timeout, "timeout (default = <env>)");
    optparse_add_unsigned(parser, 'v', "version", "NUM", 0, &version, "version (default = 1)");


	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;
	optparse_add_unsigned(parser, 'w', "window", "NUM", 0, &window, "rdp window (default = 3 packets)");
	optparse_add_unsigned(parser, 'c', "conn_timeout", "NUM", 0, &conn_timeout, "rdp connection timeout (default = 10 seconds)");
	optparse_add_unsigned(parser, 'p', "packet_timeout", "NUM", 0, &packet_timeout, "rdp packet timeout (default = 5 seconds)");
	optparse_add_unsigned(parser, 'k', "ack_timeout", "NUM", 0, &ack_timeout, "rdp max acknowledgement interval (default = 2 seconds)");
	optparse_add_unsigned(parser, 'a', "ack_count", "NUM", 0, &ack_count, "rdp ack for each (default = 2 packets)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	/* Expect filename */
	if (++argi >= slash->argc) {
		printf("missing filename\n");
		return SLASH_EINVAL;
	}

	char * file;
	file = slash->argv[argi];

	/* Expect address */
	if (++argi >= slash->argc) {
		printf("missing address\n");
		return SLASH_EINVAL;
	}

	char * endptr;
	uint64_t address = strtoul(slash->argv[argi], &endptr, 16);
	if (*endptr != '\0') {
		printf("Failed to parse address\n");
		return SLASH_EUSAGE;
	}

	printf("Upload from %s to node %u addr 0x%"PRIX64" with timeout %u, version %u\n", file, node, address, timeout, version);

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

	vmem_upload(node, timeout, address, data, size, version);

	return SLASH_SUCCESS;
}
slash_command(upload, vmem_client_slash_upload, "<file> <address>", "Upload from FILE to VMEM");
