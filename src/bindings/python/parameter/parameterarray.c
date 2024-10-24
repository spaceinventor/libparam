/*
 * parameterarray.c
 *
 * Contains the ParameterArray subclass.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "parameterarray.h"

#include "../sipyparam.h"
#include "../utils.h"


static PyObject * ParameterArray_GetItem(ParameterObject *self, PyObject *item) {

	if (!PyLong_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "Index must be an integer.");
        return NULL;
	}

	// _PyLong_AsInt is dependant on the Py_LIMITED_API macro, hence the underscore prefix.
	int index = _PyLong_AsInt(item);

	// We can't use the fact that _PyLong_AsInt() returns -1 for error,
	// because it may be our desired index. So we check for an exception instead.
	if (PyErr_Occurred())
		return NULL;  // 'Reraise' the current exception.

	/* TODO Kevin: How should we handle remote/cached when using indexed access? */
	return _sipyparam_util_get_single(self->param, index, 1, self->host, self->timeout, self->retries, self->paramver, -1);
}

static int ParameterArray_SetItem(ParameterObject *self, PyObject* item, PyObject* value) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete parameter array indexes.");
        return -1;
    }

	if (!PyLong_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "Index must be an integer.");
        return -2;
	}

	// _PyLong_AsInt is dependant on the Py_LIMITED_API macro, hence the underscore prefix.
	int index = _PyLong_AsInt(item);

	// We can't use the fact that _PyLong_AsInt() returns -1 for error,
	// because it may be our desired index. So we check for an exception instead.
	if (PyErr_Occurred())
		return -3;  // 'Reraise' the current exception.

	// _sipyparam_util_set_single() uses negative numbers for exceptions,
	// so we just return its return value.
#if 0  /* TODO Kevin: When should we use queues with the new cmd system? */
	param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
#endif
	return _sipyparam_util_set_single(self->param, value, index, self->host, self->timeout, self->retries, self->paramver, 1, sipyparam_dfl_verbose);
}

static Py_ssize_t ParameterArray_length(ParameterObject *self) {
	// We currently raise an exception when getting len() of non-array type parameters.
	// This stops PyCharm (Perhaps other IDE's) from showing their length as 0. ¯\_(ツ)_/¯
	if (self->param->array_size <= 1) {
		PyErr_SetString(PyExc_AttributeError, "Non-array type parameter is not subscriptable");
		return -1;
	}
	return self->param->array_size;
}

static PyMappingMethods ParameterArray_as_mapping = {
    (lenfunc)ParameterArray_length,
    (binaryfunc)ParameterArray_GetItem,
    (objobjargproc)ParameterArray_SetItem
};

PyTypeObject ParameterArrayType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "sipyparam.ParameterArray",
	.tp_doc = "Wrapper utility class for libparam array parameters.",
	.tp_basicsize = sizeof(ParameterArrayObject),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	// .tp_new = Parameter_new,
	// .tp_dealloc = (destructor)Parameter_dealloc,
	// .tp_getset = Parameter_getsetters,
	// .tp_str = (reprfunc)Parameter_str,
	.tp_as_mapping = &ParameterArray_as_mapping,
	// .tp_richcompare = (richcmpfunc)Parameter_richcompare,
	.tp_base = &ParameterType,
};

