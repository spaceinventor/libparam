/*
 * param_py.h
 *
 * Wrappers for lib/param/src/param/param_slash.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

PyObject * sipyparam_param_get(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_set(PyObject * self, PyObject * args, PyObject * kwds);


PyObject * sipyparam_param_cmd(PyObject * self, PyObject * args);

PyObject * sipyparam_param_cmd_add(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_cmd_run(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_pull(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_cmd_new(PyObject * self, PyObject * args, PyObject * kwds);

PyObject * sipyparam_param_cmd_done(PyObject * self, PyObject * args);
