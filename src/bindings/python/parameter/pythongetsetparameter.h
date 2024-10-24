/*
 * pythonparameter.h
 *
 * Contains the PythonParameter Parameter subclass.
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <param/param.h>

#include "pythonparameter.h"

/* TODO Kevin: If we wish to remain in line with PyExc_ParamCallbackError,
    Getter/Setter exceptions should be created. */
//extern PyObject * PyExc_ParamGetterError;
//extern PyObject * PyExc_ParamSetterError;

typedef struct {
    PythonParameterObject parameter_object;
    PyObject *getter_func;
    PyObject *setter_func;

    /* Every GetSetParameter instance allocates its own vmem.
        This vmem is passed to its read/write,
        which allows us to work our way back to the PythonGetSetParameterObject,
        based on knowing the offset of the vmem field.
        The PythonGetSetParameterObject is needed to call the Python getter/setter funcs. */
    /* NOTE: PythonParameter (our baseclass) uses param_list_create_remote() to create its ->param,
        which both allocates and assign a vmem to ->param->vmem.
        This vmem will be overridden by the one you see below,
        but it will still be freed by param_list_remove_specific().
        It would be nice to reuse it,
        but it probably can't help us find our PythonGetSetParameterObject */
    vmem_t vmem_heap;  
} PythonGetSetParameterObject;

extern PyTypeObject PythonGetSetParameterType;
