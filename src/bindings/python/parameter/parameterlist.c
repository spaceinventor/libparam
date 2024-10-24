/*
 * parameterlist.c
 *
 * Contains the ParameterList class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "parameterlist.h"

#include <param/param_queue.h>
#include <param/param_server.h>
#include <param/param_client.h>

#include "../sipyparam.h"
#include "../utils.h"
#include "parameter.h"


/**
 * @brief Checks that the argument is a Parameter object, before calling list.append().
 * Remember to PY_DECREF() the result if used internally.
 * @return Py_None
 */
PyObject * ParameterList_append(PyObject * self, PyObject * args) {
	PyObject * obj;

	if (!PyArg_ParseTuple(args, "O", &obj)) {
		return NULL;
	}

	if (!PyObject_TypeCheck(obj, &ParameterType)) {
		char errstr[40 + strlen(Py_TYPE(self)->tp_name)];
		sprintf(errstr, "%ss can only contain Parameters.", Py_TYPE(self)->tp_name);
		PyErr_SetString(PyExc_TypeError, errstr);
		return NULL;
	}

	//PyList_Append(self, obj);

	/* 
	Finding the name in the superclass is likely not nearly as efficient 
	as calling list.append() directly. But it is more flexible.
	*/
	PyObject * func_name AUTO_DECREF = Py_BuildValue("s", "append");
	Py_XDECREF(call_super_pyname_lookup(self, func_name, args, NULL));

	Py_RETURN_NONE;
}

/* Pulls all Parameters in the list as a single request. */
static PyObject * ParameterList_pull(ParameterListObject *self, PyObject *args, PyObject *kwds) {
	
	CSP_INIT_CHECK()

	unsigned int node = sipyparam_dfl_node;
	unsigned int timeout = sipyparam_dfl_timeout;
	int paramver = 2;

	static char *kwlist[] = {"node", "timeout", "paramver", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIi", kwlist, &node, &timeout, &paramver))
		return NULL;  // TypeError is thrown

	void * queuebuffer CLEANUP_FREE = malloc(PARAM_SERVER_MTU);
	param_queue_t queue = { };
	param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);

	int seqlen = PySequence_Fast_GET_SIZE(self);

	for (int i = 0; i < seqlen; i++) {

		PyObject *item = PySequence_Fast_GET_ITEM(self, i);

		if(!item) {
			PyErr_SetString(PyExc_RuntimeError, "Iterator went outside the bounds of the list.");
            return NULL;
        }

		if (!PyObject_TypeCheck(item, &ParameterType)) {  // Sanity check
			fprintf(stderr, "Skipping non-parameter object (of type: %s) in Parameter list.", item->ob_type->tp_name);
			continue;
		}

		if (param_queue_add(&queue, ((ParameterObject *)item)->param, -1, NULL) < 0) {
			PyErr_SetString(PyExc_MemoryError, "Queue full");
			return NULL;
		}

	}

	if (param_pull_queue(&queue, CSP_PRIO_NORM, 0, node, timeout)) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}

/* Pushes all Parameters in the list as a single request. */
static PyObject * ParameterList_push(ParameterListObject *self, PyObject *args, PyObject *kwds) {

	CSP_INIT_CHECK()
	
	unsigned int node = sipyparam_dfl_node;
	unsigned int timeout = sipyparam_dfl_timeout;
	uint32_t hwid = 0;
	int paramver = 2;

	static char *kwlist[] = {"node", "timeout", "hwid", "paramver", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|IIIi", kwlist, &node, &timeout, &hwid, &paramver))
		return NULL;  // TypeError is thrown

	void * queuebuffer CLEANUP_FREE = malloc(PARAM_SERVER_MTU);
	param_queue_t queue = { };
	param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);

	// Would likely segfault if 'self' is not a sequence, not that this ever seems be possible.
	// Python seems to perform a sanity check on the type of 'self' before running the method.
	// Source: cpython/Objects/descrobject.c->descr_check().
	int seqlen = PySequence_Fast_GET_SIZE(self);

	for (int i = 0; i < seqlen; i++) {

		PyObject *item = PySequence_Fast_GET_ITEM(self, i);

		if(!item) {
			PyErr_SetString(PyExc_RuntimeError, "Iterator went outside the bounds of the list.");
            return NULL;
        }

		if (!PyObject_TypeCheck(item, &ParameterType)) {  // Sanity check
			fprintf(stderr, "Skipping non-parameter object (of type: %s) in Parameter list.", item->ob_type->tp_name);
			continue;
		}

		if (param_queue_add(&queue, ((ParameterObject *)item)->param, -1, ((ParameterObject *)item)->param->addr) < 0) {
			PyErr_SetString(PyExc_MemoryError, "Queue full");
			return NULL;
		}
	}

	if (param_push_queue(&queue, 1, node, timeout, hwid, false) < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyMethodDef ParameterList_methods[] = {
    {"append", (PyCFunction)ParameterList_append, METH_VARARGS,
     PyDoc_STR("Add a Parameter to the list.")},
	{"pull", (PyCFunction)ParameterList_pull, METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Pulls all Parameters in the list as a single request.")},
	{"push", (PyCFunction)ParameterList_push, METH_VARARGS | METH_KEYWORDS,
     PyDoc_STR("Pushes all Parameters in the list as a single request.")},
    {NULL, NULL, 0, NULL}
};

static int ParameterList_init(ParameterListObject *self, PyObject *args, PyObject *kwds) {

	int seqlen = PySequence_Fast_GET_SIZE(args);
	PyObject *iterobj = args;  // Which object should we iterate over to get inital members.

	// This .__init__() function accepts either an iterable or (Python) *args as its initial members.
	if (seqlen == 1) {
		PyObject *argitem = PySequence_Fast_GET_ITEM(args, 0);
		if (PySequence_Check(argitem)) {
			// This likely fails to have its intended effect if args is some weird nested iterable like:
			// (((((1)), 2))), but this initializes a ParameterList incorrectly anyway.
			return ParameterList_init(self, argitem, kwds);
		}
		else if (PyIter_Check(argitem))
			iterobj = argitem;
	}

	// Call list init without any arguments, in case it's needed.
	if (PyList_Type.tp_init((PyObject *) self, PyTuple_Pack(0), kwds) < 0)
        return -1;

	PyObject *iter AUTO_DECREF = PyObject_GetIter(iterobj);
	PyObject *item;

	while ((item = PyIter_Next(iter)) != NULL) {

		PyObject * valuetuple AUTO_DECREF = PyTuple_Pack(1, item);
		ParameterList_append((PyObject *)self, valuetuple);

		Py_DECREF(item);

		if (PyErr_Occurred())  // Likely to happen when we fail to append an object.
			return -1;
	}

    return 0;
}

/* Subclass of the Python list which implements an interface to libparam's queue API. 
   This makes some attempts to restricts and validate its contents to be parameters.
   This is generally considered unpythonic however, and shouldn't be relied upon */
PyTypeObject ParameterListType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "sipyparam.ParameterList",
	.tp_doc = "Parameter list class with an interface to libparam's queue API.",
	.tp_basicsize = sizeof(ParameterListObject),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_init = (initproc)ParameterList_init,
	// .tp_dealloc = (destructor)ParameterList_dealloc,
	.tp_methods = ParameterList_methods,
};
