/*
 * vmem_client_py.c
 *
 * Wrappers for lib/param/src/vmem/vmem_client_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <vmem/vmem_server.h>
#include <vmem/vmem_client.h>

#include "vmem_client_py.h"

#include "../sipyparam.h"

PyObject * sipyparam_vmem_download(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

	unsigned int node = sipyparam_dfl_node;
	unsigned int timeout = sipyparam_dfl_timeout;
	unsigned int version = 2;

	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;
	unsigned int address = 0;
	unsigned int length = 0;

    static char *kwlist[] = {"address", "length", "node", "window", "conn_timeout", "packet_timeout", "ack_timeout", "ack_count", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "II|IIIIII", kwlist, &address, &length, &node, &window, &conn_timeout, &packet_timeout, &ack_timeout, &ack_count))
		return NULL;  // TypeError is thrown

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	printf("Downloading from: %08"PRIX32"\n", address);
	char *odata = malloc(length);

	vmem_download(node,timeout,address,length,odata,version,1);

	PyObject * vmem_data = PyBytes_FromStringAndSize(odata, length);

	free(odata);

	return vmem_data;

}

PyObject * sipyparam_vmem_upload(PyObject * self, PyObject * args, PyObject * kwds) {
	
	CSP_INIT_CHECK()

	unsigned int node = sipyparam_dfl_node;
	unsigned int timeout = sipyparam_dfl_timeout;
	unsigned int version = 2;

	/* RDPOPT */
	unsigned int window = 3;
	unsigned int conn_timeout = 10000;
	unsigned int packet_timeout = 5000;
	unsigned int ack_timeout = 2000;
	unsigned int ack_count = 2;
	unsigned int address = 0;
	PyObject * data_in = NULL;

    static char *kwlist[] = {"address", "data_in", "node", "window", "conn_timeout", "packet_timeout", "ack_timeout", "ack_count", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "IO|IIIIII", kwlist, &address, &data_in, &node, &window, &conn_timeout, &packet_timeout, &ack_timeout, &ack_count))
		return NULL;  // TypeError is thrown

	printf("Setting rdp options: %u %u %u %u %u\n", window, conn_timeout, packet_timeout, ack_timeout, ack_count);
	csp_rdp_set_opt(window, conn_timeout, packet_timeout, 1, ack_timeout, ack_count);

	printf("Uploading from: %08"PRIX32"\n", address);
	char *idata = NULL;
	Py_ssize_t idata_len = 0;

	PyBytes_AsStringAndSize(data_in, &idata, &idata_len);
	if (idata_len > 0 && idata != NULL) {
		vmem_upload(node, timeout, address, idata, idata_len, version);
	}

	Py_RETURN_NONE;

}


PyObject * sipyparam_param_vmem(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

	unsigned int node = sipyparam_dfl_node;
	unsigned int timeout = sipyparam_dfl_timeout;
	unsigned int version = 2;

	static char *kwlist[] = {"node", "timeout", "version", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iii", kwlist, &node, &timeout, &version))
		return NULL;  // Raises TypeError.

	printf("Requesting vmem list from node %u timeout %u version %d\n", node, timeout, version);

	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Failed to get CSP buffer");
		return NULL;
	}

	vmem_request_t * request = (void *) packet->data;
	request->version = version;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		csp_close(conn);
		return NULL;
	}

	PyObject * list_string = PyUnicode_New(0, 0);

	if (request->version == 2) {
		for (vmem_list2_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			char buf[500];
			snprintf(buf, sizeof(buf), " %u: %-5.5s 0x%lX - %u typ %u\r\n", vmem->vmem_id, vmem->name, be64toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
			printf("%s", buf);
			PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
		}
	} else {
		for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
			char buf[500];
			snprintf(buf, sizeof(buf), " %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
			printf("%s", buf);
			PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
		}
	}

	csp_buffer_free(packet);
	csp_close(conn);

	return list_string;
}