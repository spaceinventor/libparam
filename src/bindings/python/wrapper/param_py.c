/*
 * param_py.c
 *
 * Wrappers for lib/param/src/param/param_slash.c
 *
 */

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "sipyparamconfig.h"

#include <param/param.h>
#include <param/param_queue.h>
#include <param/param_client.h>
#include <param/param_server.h>

#include "../sipyparam.h"
#include "../utils.h"

#include "param_py.h"

#ifdef SIPYPARAM_HAVE_SLASH
/* Assumes that param:slash if we have slash, and that we want to use the queue from there.
	This makes slash_execute() use the same queue as SiPyParam */
/* param_slash.h is not exposed, but we need the queue from there, so please close your eyes */
extern param_queue_t param_queue;
#else
static char queue_buf[PARAM_SERVER_MTU];
param_queue_t param_queue = { .buffer = queue_buf, .buffer_size = PARAM_SERVER_MTU, .type = PARAM_QUEUE_TYPE_EMPTY, .version = 2 };
#endif

PyObject * sipyparam_param_get(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int node = sipyparam_dfl_node;
	int server = INT_MIN;
	int paramver = 2;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as array Parameters should be backwards subscriptable like lists.
	int timeout = sipyparam_dfl_timeout;
	int retries = 1;
	int verbose = sipyparam_dfl_verbose;

	static char *kwlist[] = {"param_identifier", "node", "server", "paramver", "offset", "timeout", "retries", "verbose", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iiiiiii", kwlist, &param_identifier, &node, &server, &paramver, &offset, &timeout, &retries, &verbose))
		return NULL;  // TypeError is thrown

	param_t *param = _sipyparam_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	/* Select destination, host overrides parameter node */
	int dest = node;
	if (server > 0)
		dest = server;

	// _sipyparam_util_get_single() and _sipyparam_util_get_array() will return NULL for exceptions, which is fine with us.
	if (param->array_size > 1 && param->type != PARAM_TYPE_STRING)
		return _sipyparam_util_get_array(param, 1, dest, timeout, retries, paramver, verbose);
	return _sipyparam_util_get_single(param, offset, 1, dest, timeout, retries, paramver, verbose);
}

PyObject * sipyparam_param_set(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	PyObject * value;
	int node = sipyparam_dfl_node;
	int server = INT_MIN;
	int paramver = 2;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.
	int timeout = sipyparam_dfl_timeout;
	int retries = 1;
	int verbose = 2;  // TODO Kevin: 2 chosen over sipyparam_dfl_verbose, as this is the default in CSH

	static char *kwlist[] = {"param_identifier", "value", "node", "server", "paramver", "offset", "timeout", "retries", "verbose", NULL};
	
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|iiiiiii", kwlist, &param_identifier, &value, &node, &server, &paramver, &offset, &timeout, &retries, &verbose)) {
		return NULL;  // TypeError is thrown
	}

	param_t *param = _sipyparam_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	int dest = node;
	if (server > 0)
		dest = server;

	if((PyIter_Check(value) || PySequence_Check(value)) && !PyObject_TypeCheck(value, &PyUnicode_Type)) {
		if (_sipyparam_util_set_array(param, value, dest, timeout, retries, paramver, verbose))
			return NULL;  // Raises one of many possible exceptions.
	} else {
#if 0  /* TODO Kevin: When should we use queues with the new cmd system? */
		param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
#endif
		if (_sipyparam_util_set_single(param, value, offset, dest, timeout, retries, paramver, 1, verbose))
			return NULL;  // Raises one of many possible exceptions.
		param_print(param, -1, NULL, 0, verbose, 0);
	}

	Py_RETURN_NONE;
}

PyObject * sipyparam_param_cmd(PyObject * self, PyObject * args) {

	if (param_queue.type == PARAM_QUEUE_TYPE_EMPTY) {
		printf("No active command\n");
	} else {
		printf("Current command size: %d bytes\n", param_queue.used);
		param_queue_print(&param_queue);
	}

	Py_RETURN_NONE;

}

PyObject * sipyparam_param_pull(PyObject * self, PyObject * args, PyObject * kwds) {

	CSP_INIT_CHECK()

	unsigned int node = sipyparam_dfl_node;
	unsigned int timeout = sipyparam_dfl_timeout;
	PyObject * include_mask_obj = NULL;
	PyObject * exclude_mask_obj = NULL;
	int paramver = 2;

	static char *kwlist[] = {"node", "timeout", "include_mask", "exclude_mask", "paramver", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIOOi", kwlist, &node, &timeout, &include_mask_obj, &exclude_mask_obj, &paramver)) {
		return NULL;
	}

	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;

	if (include_mask_obj != NULL) {
		if (sipyparam_parse_param_mask(include_mask_obj, &include_mask) != 0) {
			return NULL;  // Exception message set by sipyparam_parse_param_mask()
		}
	}

	if (exclude_mask_obj != NULL) {
		if (sipyparam_parse_param_mask(exclude_mask_obj, &exclude_mask) != 0) {
			return NULL;  // Exception message set by sipyparam_parse_param_mask()
		}
	}

	int param_pull_res;
	Py_BEGIN_ALLOW_THREADS;

	param_pull_res = param_pull_all(CSP_PRIO_NORM, 1, node, include_mask, exclude_mask, timeout, paramver);
	Py_END_ALLOW_THREADS;
	if (param_pull_res) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	Py_RETURN_NONE;
}

PyObject * sipyparam_param_cmd_new(PyObject * self, PyObject * args, PyObject * kwds) {

	char *type;
	char *name;
	int paramver = 2;

	static char *kwlist[] = {"type", "name", "paramver", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ss|i", kwlist, &type, &name, &paramver, &paramver)) {
		return NULL;
	}

	/* Set/get */
	if (strncmp(type, "get", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_GET;
	} else if (strncmp(type, "set", 4) == 0) {
		param_queue.type = PARAM_QUEUE_TYPE_SET;
	} else {
		PyErr_SetString(PyExc_ValueError, "Must specify 'get' or 'set'");
		return NULL;
	}

	/* Command name */
	strncpy(param_queue.name, name, sizeof(param_queue.name) - 1);

	param_queue.used = 0;
	param_queue.version = paramver;

	printf("Initialized new command: %s\n", name);

	Py_RETURN_NONE;
}

PyObject * sipyparam_param_cmd_done(PyObject * self, PyObject * args) {
	param_queue.type = PARAM_QUEUE_TYPE_EMPTY;
	Py_RETURN_NONE;
}