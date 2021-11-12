// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdio.h>


#include <param/param.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>
#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/arch/csp_thread.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>
#include <param/param.h>
#include <param/param_collector.h>
#include <param/param_queue.h>

#include <sys/types.h>
#include "../../param/param_string.h"


#define PARAMID_CSP_RTABLE					 12


VMEM_DEFINE_FILE(csp, "csp", "cspcnf.vmem", 120);
VMEM_DEFINE_FILE(params, "param", "params.csv", 50000);
VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(crypto, "crypto", "crypto.csv", 50000);
VMEM_DEFINE_FILE(tfetch, "tfetc", "tfetch.vmem", 120);


PARAM_DEFINE_STATIC_VMEM(PARAMID_CSP_RTABLE,      csp_rtable,        PARAM_TYPE_STRING, 64, 0, PM_SYSCONF, NULL, "", csp, 0, NULL);


static pthread_t router_handle;
void * router_task(void * param) {
	while(1) {
		csp_route_work();
	}
}

static uint8_t _csp_initialized = 0;

param_queue_t param_queue_set = { };
param_queue_t param_queue_get = { };
static int default_node = -1;
static int autosend = 1;
static int paramver = 2;

static PyObject * Error = NULL;

//static int PARAM_POINTER_HAS_BEEN_FREED = 0;  // used to indicate pointer has been freed, because a NULL pointer can't be set.

static PyObject * pyparam_param_get(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError, 
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int host = -1;
	int node = default_node;
	int offset = -1;

	if (!PyArg_ParseTuple(args, "O|iii", &param_identifier, &host, &node, &offset)) {
		return NULL;  // TypeError is thrown
	}


	int is_string = PyUnicode_Check(param_identifier);
	int is_int = PyLong_Check(param_identifier);

	param_t * param;

	if (is_string)
		param = param_list_find_name(node, (char*)PyUnicode_AsUTF8(param_identifier));
	else if (is_int)
		param = param_list_find_id(node, (int)PyLong_AsLong(param_identifier));
	else {
		PyErr_SetString(PyExc_TypeError, 
			"First argument must be either an int or string identifier of the intended parameter.");
		return 0;
	}

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	/* Remote parameters are sent to a queue or directly */
	int result = 0;
	if (param->node != PARAM_LIST_LOCAL) {

		if (autosend) {
			if (host != -1) {
				result = param_pull_single(param, offset, 1, host, 1000, paramver);
			} else if (node != -1) {
				result = param_pull_single(param, offset, 1, node, 1000, paramver);
			}
		} else {
			if (!param_queue_get.buffer) {
			    param_queue_init(&param_queue_get, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);
			}
			if (param_queue_add(&param_queue_get, param, offset, NULL) < 0)
				printf("Queue full\n");
			param_queue_print(&param_queue_get);
			Py_RETURN_NONE;
		}

	} else if (autosend) {
		param_print(param, -1, NULL, 0, 0);
	} else {
		if (!param_queue_get.buffer) {
			param_queue_init(&param_queue_get, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);
		}
		if (param_queue_add(&param_queue_get, param, offset, NULL) < 0)
			printf("Queue full\n");
		param_queue_print(&param_queue_get);
		Py_RETURN_NONE;
	}

	if (result < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return 0;
	}

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_set(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError, 
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	char * strvalue;
	int host = -1;
	int node = default_node;
	int offset = -1;

	if (!PyArg_ParseTuple(args, "Os|iii", &param_identifier, &strvalue, &host, &node, &offset)) {
		return NULL;  // TypeError is thrown
	}

	// printf("strvalue is:\t%s\n", strvalue);

	// printf("\nParameters before push:\n");
	// param_pull_all(1, host, 0xFFFFFFFF, 0, 1000, paramver);
	// printf("\n");

	// {
	// 	/* Print all known parameters */  // TODO Kevin: For debug purposes.
	// 	param_t * param;
	// 	param_list_iterator i = {};
	// 	while ((param = param_list_iterate(&i)) != NULL) {
	// 		printf("param name:\t%s\tparam node:\t%i\n", param->name, param->node);
	// 	}
	// }


	int is_string = PyUnicode_Check(param_identifier);
	int is_int = PyLong_Check(param_identifier);

	param_t * param;

	if (is_string)
		param = param_list_find_name(node, (char*)PyUnicode_AsUTF8(param_identifier));
	else if (is_int)
		param = param_list_find_id(node, (int)PyLong_AsLong(param_identifier));
	else {
		PyErr_SetString(PyExc_TypeError, 
			"First argument must be either an int or string identifier of the intended parameter.");
		return 0;
	}

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	// printf("Found param:\t%i\n", param->id);

	char valuebuf[128] __attribute__((aligned(16))) = { };
	param_str_to_value(param->type, strvalue, valuebuf);

	// printf("As string:\t");
	// for (size_t i = 0; i < 128; i++)
	// {
	// 	printf("%c", valuebuf[i]);
	// }
	// printf("\n");

	// printf("As ints:\t");
	// for (size_t i = 0; i < 128; i++)
	// {
	// 	printf("%i", valuebuf[i]);
	// }
	// printf("\n");
	

	/* Remote parameters are sent to a queue or directly */
	int result = 0;
	if (param->node != PARAM_LIST_LOCAL) {

		if ((node != -1) && (autosend)) {
			result = param_push_single(param, offset, valuebuf, 1, node, 1000, paramver);
		} else if (host != -1) {
			result = param_push_single(param, offset, valuebuf, 1, host, 1000, paramver);
		} else {
			if (!param_queue_set.buffer) {
				param_queue_init(&param_queue_set, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
			}
			if (param_queue_add(&param_queue_set, param, offset, valuebuf) < 0)
				printf("Queue full\n");
			param_queue_print(&param_queue_set);
			Py_RETURN_NONE;
		}
		
	} else if (autosend) {
		/* For local parameters, set immediately if autosend is enabled */
	    if (offset < 0)
			offset = 0;
		param_set(param, offset, valuebuf);
	} else {
		/* If autosend is off, queue the parameters */
		if (!param_queue_set.buffer) {
			param_queue_init(&param_queue_set, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
		}
		if (param_queue_add(&param_queue_set, param, offset, valuebuf) < 0)
			printf("Queue full\n");
		param_queue_print(&param_queue_set);
		Py_RETURN_NONE;
	}

	if (result < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return 0;
	}

	param_print(param, -1, NULL, 0, 2);
	
	// TODO Kevin: Remove debug prints.
	// printf("Param is named:\t%s\n", param->name);
	// printf("is_string=%i\nis_int=%i\n", is_string, is_int);

	// printf("\nParameters after push:\n");
	// param_pull_all(1, host, 0xFFFFFFFF, 0, 1000, paramver);

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_push(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError, 
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int node = 0;
	unsigned int timeout = 100;

	if (!PyArg_ParseTuple(args, "I|I", &node, &timeout)) {
		return NULL;  // TypeError is thrown
	}

	if (param_push_queue(&param_queue_set, 1, node, timeout) < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_pull(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError, 
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int host = 0;
	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;
	unsigned int timeout = 1000;

	char * _str_include_mask;
	char * _str_exclude_mask;

	if (!PyArg_ParseTuple(args, "I|ssI", &host, &_str_include_mask, &_str_exclude_mask, &timeout)) {
		return NULL;
	}

	if (_str_include_mask != NULL)
		include_mask = param_maskstr_to_mask(_str_include_mask);
	if (_str_exclude_mask != NULL)
		exclude_mask = param_maskstr_to_mask(_str_exclude_mask);

	int result = -1;
	if (param_queue_get.used == 0) {
		result = param_pull_all(1, host, include_mask, exclude_mask, timeout, paramver);
	} else {
		result = param_pull_queue(&param_queue_get, 1, host, timeout);
	}

	if (result) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_clear(PyObject * self, PyObject * args) {

	param_queue_get.used = 0;
	param_queue_set.used = 0;
    param_queue_get.version = paramver;
    param_queue_set.version = paramver;
	printf("Queue cleared\n");
	Py_RETURN_NONE;

}

static PyObject * pyparam_param_node(PyObject * self, PyObject * args) {

	unsigned int node = -1;

	if (!PyArg_ParseTuple(args, "|I", &node)) {
		return NULL;  // TypeError is thrown
	}

	if (node == -1)
		printf("Default node = %d\n", default_node);
	else {
		default_node = node;
		printf("Set default node to %d\n", default_node);
	}

	Py_RETURN_NONE;

}

static PyObject * pyparam_param_paramver(PyObject * self, PyObject * args) {

	// Not sure if the static paramver would be set to NULL, when nothing is passed.
	int _paramver = -1;  

	if (!PyArg_ParseTuple(args, "|I", &_paramver)) {
		return NULL;  // TypeError is thrown
	}

	if (_paramver == -1)
		printf("Parameter client version = %d\n", paramver);
	else {
		paramver = _paramver;
		printf("Set parameter client version to %d\n", paramver);
	}

	Py_RETURN_NONE;

}

static PyObject * pyparam_param_autosend(PyObject * self, PyObject * args) {

	// Not sure if the static autosend would be set to NULL, when nothing is passed.
	unsigned int _autosend = -1;

	if (!PyArg_ParseTuple(args, "|I", &_autosend)) {
		return NULL;  // TypeError is thrown
	}

	if (_autosend == -1)
		printf("autosend = %d\n", autosend);
	else {
		autosend = _autosend;
		printf("Set autosend to %d\n", autosend);
	}

	Py_RETURN_NONE;

}

static PyObject * pyparam_param_queue(PyObject * self, PyObject * args) {

	if ( (param_queue_get.used == 0) && (param_queue_set.used == 0) ) {
		printf("Nothing queued\n");
	}
	if (param_queue_get.used > 0) {
		printf("Get Queue\n");
		param_queue_print(&param_queue_get);
	}
	if (param_queue_set.used > 0) {
		printf("Set Queue\n");
		param_queue_print(&param_queue_set);
	}

	Py_RETURN_NONE;

}


static PyObject * pyparam_param_list(PyObject * self, PyObject * args) {

	uint32_t mask = 0xFFFFFFFF;

	char * _str_mask;

	if (!PyArg_ParseTuple(args, "|s", &_str_mask)) {
		return NULL;
	}

	if (_str_mask != NULL)
		mask = param_maskstr_to_mask(_str_mask);

	param_list_print(mask);

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_list_download(PyObject * self, PyObject * args) {

	unsigned int node;
    unsigned int timeout = 1000;
    unsigned int version = 2;

	if (!PyArg_ParseTuple(args, "I|II", &node, &timeout, &version)) {
		return NULL;  // TypeError is thrown
	}

	param_list_download(node, timeout, version);

	Py_RETURN_NONE;

}


static PyObject * pyparam_csp_ping(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError, 
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int node;
	unsigned int timeout = 1000;
	unsigned int size = 1;

	if (!PyArg_ParseTuple(args, "I|II", &node, &timeout, &size)) {
		return NULL;  // TypeError is thrown
	}

	printf("Ping node %u size %u timeout %u: ", node, size, timeout);

	int result = csp_ping(node, timeout, size, CSP_O_CRC32);

	if (result >= 0) {
		printf("Reply in %d [ms]\n", result);
	} else {
		printf("No reply\n");
	}

	Py_RETURN_NONE;

}

static PyObject * pyparam_csp_ident(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError, 
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int node;
	unsigned int timeout = 1000;
	unsigned int size = 1;

	if (!PyArg_ParseTuple(args, "I|II", &node, &timeout, &size)) {
		return NULL;  // TypeError is thrown
	}

	struct csp_cmp_message message;

	if (csp_cmp_ident(node, timeout, &message) != CSP_ERR_NONE) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	printf("%s\n%s\n%s\n%s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);

	Py_RETURN_NONE;
	
}


static PyObject * pyparam_csp_init(PyObject * self, PyObject * args) {

	// printf("Init address %li\n", &_csp_initialized);

	if (_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError, 
			"Cannot initialize multiple instances of libparam bindings. Please use a previous binding.");
			return 0;
	}

	csp_conf.address = 1;
	csp_conf.version = 2;
	csp_conf.hostname = "python_bindings";
	csp_conf.model = "linux";

	uint16_t csp_port = PARAM_PORT_SERVER;

	// TODO Kevin: These should probably be parsed as keyword arguments.
	if (!PyArg_ParseTuple(args, "|ibssi", &csp_conf.address, &csp_conf.hostname, &csp_conf.model, &csp_conf.revision, &csp_port)) {
		return NULL;  // TypeError is thrown
	}

	/* Get csp config from file */
	vmem_file_init(&vmem_csp);

	/* Parameters */
	vmem_file_init(&vmem_params);
	param_list_store_vmem_load(&vmem_params);

	
	csp_init();


	pthread_create(&router_handle, NULL, &router_task, NULL);

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);

	char saved_rtable[csp_rtable.array_size];
	char * rtable = NULL;
	csp_iface_t * default_iface = NULL;

	if (!rtable) {
		/* Read routing table from parameter system */
		param_get_string(&csp_rtable, saved_rtable, csp_rtable.array_size);
		rtable = saved_rtable;
	}

	if (csp_rtable_check(rtable)) {
		int error = csp_rtable_load(rtable);
		if (error < 1) {
			csp_log_error("csp_rtable_load(%s) failed, error: %d", rtable, error);
			//exit(1);
		}
	} else if (default_iface) {
		printf("Setting default route to %s\n", default_iface->name);
		csp_rtable_set(0, 0, default_iface, CSP_NO_VIA_ADDRESS);
	} else {
		printf("No routing defined\n");
	}

	csp_socket_t *sock_csh = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_csh, csp_service_handler);
	csp_bind(sock_csh, CSP_ANY);

	csp_socket_t *sock_param = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_param, param_serve);
	csp_bind(sock_param, csp_port);

	csp_thread_handle_t vmem_handle;
	csp_thread_create(vmem_server_task, "vmem", 2000, NULL, 1, &vmem_handle);

	/* Start a collector task */
	vmem_file_init(&vmem_col);
	pthread_t param_collector_handle;
	pthread_create(&param_collector_handle, NULL, &param_collector_task, NULL);

	/* Crypto magic */
	vmem_file_init(&vmem_crypto);
	// crypto_key_refresh();

	/* Test of time fetch */
	vmem_file_init(&vmem_tfetch);
	// tfetch_onehz();
	
	_csp_initialized = 1;
	Py_RETURN_NONE;

}

static PyMethodDef methods[] = {

	/* Converted Slash/Satctl commands from param/param_slash.c */
	{"set", 		pyparam_param_set, 			METH_VARARGS, 	""},
	{"get", 		pyparam_param_get, 			METH_VARARGS, 	""},
	{"push", 		pyparam_param_push, 		METH_VARARGS, 	""},
	{"pull", 		pyparam_param_pull, 		METH_VARARGS, 	""},
	{"clear", 		pyparam_param_clear, 		METH_NOARGS, 	""},
	{"node", 		pyparam_param_node, 		METH_VARARGS, 	""},
	{"paramver", 	pyparam_param_paramver, 	METH_VARARGS, 	""},
	{"autosend", 	pyparam_param_autosend, 	METH_VARARGS, 	""},
	{"queue", 		pyparam_param_queue, 		METH_NOARGS, 	""},

	/* Converted Slash/Satctl commands from param/param_list_slash.c */
	{"list", 		pyparam_param_list, 		METH_VARARGS, 	""},
	{"list_download", pyparam_param_list_download, METH_VARARGS, ""},

	/* Converted Slash/Satctl commands from slash_csp.c */
	/* Including these here is not entirely optimal, they may be removed. */
	{"ping", 		pyparam_csp_ping, 		METH_VARARGS, 		""},
	{"ident", 		pyparam_csp_ident, 		METH_VARARGS, 		""},

	/* Misc */
	{"_param_init", pyparam_csp_init, 			METH_VARARGS, 	""},

	/* sentinel */
	{NULL, NULL, 0, NULL}};

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"libparam_py3",
	"Module ",
	-1,
	methods,
	NULL,
	NULL,
	NULL,
	NULL};

PyMODINIT_FUNC PyInit_libparam_py3(void) {

	PyObject * m = PyModule_Create(&moduledef);

	/* Exceptions */
	Error = PyErr_NewException((char *)"param.Error", NULL, NULL);
	Py_INCREF(Error);

	/* Add exception object to your module */
	PyModule_AddObject(m, "Error", Error);

	return m;
}