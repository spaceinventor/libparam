/*
 * vmem_client_slash.h
 *
 *  Created on: Oct 27, 2016
 *      Author: johan
 */


#include <vmem/vmem_client.h>

#include <slash/slash.h>

static int vmem_client_slash_download(struct slash *slash)
{
	vmem_download(1, 1000, 0x10000000, NULL, 0);
	return SLASH_SUCCESS;
}
slash_command(download, vmem_client_slash_download, "<node> <timeout> <address> <file>", "Download from VMEM to FILE");

static int vmem_client_slash_upload(struct slash *slash)
{
	vmem_upload(1, 1000, 0x10000000, NULL, 0);
	return SLASH_SUCCESS;
}
slash_command(upload, vmem_client_slash_upload, "<node> <timeout> <file> <address>", "Upload from FILE to VMEM");
