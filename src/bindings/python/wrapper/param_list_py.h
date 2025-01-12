/*
 * param_list_py.c
 *
 * Wrappers for lib/param/src/param/list/param_list_slash.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * sipyparam_param_list(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_list_download(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_list_forget(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_list_save(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_list_add(PyObject * self, PyObject * args, PyObject * kwds);