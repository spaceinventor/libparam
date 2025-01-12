/*
 * spaceboot_py.c
 *
 * Wrappers for csh/src/spaceboot_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "../sipyparam.h"

#include "spaceboot_py.h"

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_client.h>

#include <vmem/vmem.h>
#include <vmem/vmem_client.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>

/* Custom exceptions */
PyObject * PyExc_ProgramDiffError;

static int ping(int node) {

	struct csp_cmp_message message = {};
	if (csp_cmp_ident(node, 3000, &message) != CSP_ERR_NONE) {
		printf("Cannot ping system\n");
		return -1;
	}
	printf("  | %s\n  | %s\n  | %s\n  | %s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);
	return 0;
}

static int reset_to_flash(int node, int flash, int times, int type) {

	param_t * boot_img[4];
	/* Setup remote parameters */
	boot_img[0] = param_list_create_remote(21, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img0", NULL, NULL, -1);
	boot_img[1] = param_list_create_remote(20, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img1", NULL, NULL, -1);
	boot_img[2] = param_list_create_remote(22, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img2", NULL, NULL, -1);
	boot_img[3] = param_list_create_remote(23, node, PARAM_TYPE_UINT8, PM_CONF, 0, "boot_img3", NULL, NULL, -1);

	printf("  Switching to flash %d\n", flash);
	printf("  Will run this image %d times\n", times);

	char queue_buf[50];
	param_queue_t queue;
	param_queue_init(&queue, queue_buf, 50, 0, PARAM_QUEUE_TYPE_SET, 2);

	uint8_t zero = 0;
	param_queue_add(&queue, boot_img[0], 0, &zero);
	param_queue_add(&queue, boot_img[1], 0, &zero);
	if (type == 1) {
		param_queue_add(&queue, boot_img[2], 0, &zero);
		param_queue_add(&queue, boot_img[3], 0, &zero);
	}
	param_queue_add(&queue, boot_img[flash], 0, &times);
	param_push_queue(&queue, CSP_PRIO_NORM, 1, node, 1000, 0, false);

	printf("  Rebooting");
	csp_reboot(node);
	int step = 25;
	int ms = 1000;
	while (ms > 0) {
		printf(".");
		fflush(stdout);
		usleep(step * 1000);
		ms -= step;
	}
	printf("\n");

	for (int i = 0; i < 4; i++)
		param_list_destroy(boot_img[i]);

	return ping(node);
}

PyObject * slash_csp_switch(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

    unsigned int slot;
	unsigned int node = sipyparam_dfl_node;
	unsigned int times = 1;

    static char *kwlist[] = {"slot", "node", "times", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|II", kwlist, &slot, &node, &times))
		return NULL;  // TypeError is thrown

	int type = 0;
	if (slot >= 2)
		type = 1;

	if (reset_to_flash(node, slot, times, type) != 0) {
        PyErr_SetString(PyExc_ConnectionError, "Cannot ping system");
        return NULL;
    }

	Py_RETURN_NONE;
}

static vmem_list_t vmem_list_find(int node, int timeout, char * name, int namelen) {
	vmem_list_t ret = {};

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_CRC32);
	if (conn == NULL)
		return ret;

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	vmem_request_t * request = (void *)packet->data;
	request->version = 1;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		printf("No response\n");
		csp_close(conn);
		return ret;
	}

	for (vmem_list_t * vmem = (void *)packet->data; (intptr_t)vmem < (intptr_t)packet->data + packet->length; vmem++) {
		// printf(" %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
		if (strncmp(vmem->name, name, namelen) == 0) {
			ret.vmem_id = vmem->vmem_id;
			ret.type = vmem->type;
			memcpy(ret.name, vmem->name, 5);
			ret.vaddr = be32toh(vmem->vaddr);
			ret.size = be32toh(vmem->size);
		}
	}

	csp_buffer_free(packet);
	csp_close(conn);

	return ret;
}

static int image_get(char * filename, char ** data, int * len) {

	/* Open file */
	FILE * fd = fopen(filename, "r");
	if (fd == NULL) {
		printf("  Cannot find file: %s\n", filename);
		return -1;
	}

	/* Read size */
	struct stat file_stat;
	fstat(fd->_fileno, &file_stat);

	/* Copy to memory:
	 * Note we ignore the memory leak because the application will terminate immediately after using the data */
	*data = malloc(file_stat.st_size);
	*len = fread(*data, 1, file_stat.st_size, fd);
	fclose(fd);

	return 0;
}

#if 0
static void upload(int node, int address, char * data, int len) {

	unsigned int timeout = 10000;
	printf("  Upload %u bytes to node %u addr 0x%x\n", len, node, address);
	vmem_upload(node, timeout, address, data, len, 1);
	printf("  Waiting for flash driver to flush\n");
	usleep(100000);
}
#endif


#define BIN_PATH_MAX_ENTRIES 10
#define BIN_PATH_MAX_SIZE 256

struct bin_info_t {
	uint32_t addr_min;
	uint32_t addr_max;
	unsigned count;
	char entries[BIN_PATH_MAX_ENTRIES][BIN_PATH_MAX_SIZE];
} bin_info;

#if 0  // TODO Kevin: When a .bin filename is specified, is_valid_binary() should be used.
// Binary file byte offset of entry point address.
// C21: 4, E70: 2C4
static const uint32_t entry_offsets[] = { 4, 0x2c4 };

static bool is_valid_binary(const char * path, struct bin_info_t * binf) {
	int len = strlen(path);
	if ((len <= 4) || (strcmp(&(path[len-4]), ".bin") != 0)) {
		return false;
	}

	char * data;
	if (image_get((char*)path, &data, &len) < 0) {
		return false;
	}

	if (binf->addr_min + len <= binf->addr_max) {
		uint32_t addr = 0;
		for (size_t i = 0; i < sizeof(entry_offsets)/sizeof(uint32_t); i++) {
			addr = *((uint32_t *) &data[entry_offsets[i]]);
			if ((binf->addr_min <= addr) && (addr <= binf->addr_max)) {
				free(data);
				return true;
			}
		}
	}
	free(data);
	return false;
}
#endif

static int upload_and_verify(int node, int address, char * data, int len) {

	unsigned int timeout = 10000;
	printf("  Upload %u bytes to node %u addr 0x%x\n", len, node, address);
	vmem_upload(node, timeout, address, data, len, 1);

	char * datain = malloc(len);
	vmem_download(node, timeout, address, len, datain, 1, 1);

	for (int i = 0; i < len; i++) {
		if (datain[i] == data[i])
			continue;
		printf("Diff at %x: %hhx != %hhx\n", address + i, data[i], datain[i]);
		free(datain);
		return -1;
	}

	free(datain);
	return 0;
}

PyObject * sipyparam_csh_program(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

    unsigned int slot;
	unsigned int node = sipyparam_dfl_node;
	char * filename = NULL;

	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;

    static char *kwlist[] = {"slot", "filename", "node", "window", "conn_timeout", "packet_timeout", "ack_timeout", "ack_count", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "Is|IIIIII", kwlist, &slot, &filename, &node, &window, &conn_timeout, &packet_timeout, &ack_timeout, &ack_count))
		return NULL;  // TypeError is thrown

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	printf("node 1 %d\n", sipyparam_dfl_node);

	char vmem_name[5];
	snprintf(vmem_name, 5, "fl%u", slot);

	printf("  Requesting VMEM name: %s...\n", vmem_name);

	vmem_list_t vmem = vmem_list_find(node, 5000, vmem_name, strlen(vmem_name));
	if (vmem.size == 0) {
		PyErr_SetString(PyExc_ConnectionError, "Failed to find vmem on subsystem\n");
		return NULL;
	} else {
		printf("  Found vmem\n");
		printf("    Base address: 0x%x\n", vmem.vaddr);
		printf("    Size: %u\n", vmem.size);
	}

    assert(filename != NULL);
    strncpy(bin_info.entries[0], filename, BIN_PATH_MAX_SIZE-1);
    bin_info.count = 0;

	printf("node 2 %d\n", sipyparam_dfl_node);

	char * path = bin_info.entries[0];

	char * data;
	int len;
	if (image_get(path, &data, &len) < 0) {
        PyErr_SetString(PyExc_IOError, "Failed to open file");
		return NULL;
	}

    printf("\033[31m\n");
    printf("ABOUT TO PROGRAM: %s\n", path);
    printf("\033[0m\n");
    if (ping(node) < 0) {
        PyErr_SetString(PyExc_ConnectionError, "No Response");
		return NULL;
	}
    printf("\n");

    if (upload_and_verify(node, vmem.vaddr, data, len) != 0) {
        PyErr_SetString(PyExc_ProgramDiffError, "Diff during download (upload/download mismatch)");
        return NULL;
    }

	Py_RETURN_NONE;
}

PyObject * slash_sps(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

    unsigned int from;
    unsigned int to;
    char * filename = NULL;
	unsigned int node = sipyparam_dfl_node;

	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;

    static char *kwlist[] = {"from", "to", "node", "window", "conn_timeout", "packet_timeout", "ack_timeout", "ack_count", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "IIs|IIIIII", kwlist, &from, &to, &node, &window, &conn_timeout, &packet_timeout, &ack_timeout, &ack_count))
		return NULL;  // TypeError is thrown

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	int type = 0;
	if (from >= 2)
		type = 1;
	if (to >= 2)
		type = 1;

	reset_to_flash(node, from, 1, type);

	char vmem_name[5];
	snprintf(vmem_name, 5, "fl%u", to);
	printf("  Requesting VMEM name: %s...\n", vmem_name);

	vmem_list_t vmem = vmem_list_find(node, 5000, vmem_name, strlen(vmem_name));
	if (vmem.size == 0) {
		PyErr_SetString(PyExc_ConnectionError, "Failed to find vmem on subsystem\n");
		return NULL;
	} else {
		printf("  Found vmem\n");
		printf("    Base address: 0x%x\n", vmem.vaddr);
		printf("    Size: %u\n", vmem.size);
	}

	assert(filename != NULL);
    strncpy(bin_info.entries[0], filename, BIN_PATH_MAX_SIZE-1);
    bin_info.count = 0;
	
	char * path = bin_info.entries[0];

    printf("\033[31m\n");
    printf("ABOUT TO PROGRAM: %s\n", path);
    printf("\033[0m\n");
    if (ping(node) < 0) {
		PyErr_SetString(PyExc_ConnectionError, "Cannot ping system");
        return NULL;
	}
    printf("\n");

	char * data;
	int len;
	if (image_get(path, &data, &len) < 0) {
		PyErr_SetString(PyExc_IOError, "Failed to open file");
		return NULL;
	}
	
	int result = upload_and_verify(node, vmem.vaddr, data, len);
	if (result != 0) {
        PyErr_SetString(PyExc_ProgramDiffError, "Diff during download (upload/download mismatch)");
        return NULL;
	}

    if (reset_to_flash(node, to, 1, type)) {
        PyErr_SetString(PyExc_ConnectionError, "Cannot ping system");
        return NULL;
    }
	
    Py_RETURN_NONE;
}
