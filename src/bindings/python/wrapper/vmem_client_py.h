/*
 * vmem_client_py.h
 *
 * Wrappers for lib/param/src/vmem/vmem_client_slash.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * sipyparam_param_vmem(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * sipyparam_vmem_download(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * sipyparam_vmem_upload(PyObject * self, PyObject * args, PyObject * kwds);