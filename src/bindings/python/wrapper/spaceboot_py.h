/*
 * spaceboot_py.h
 *
 * Wrappers for csh/src/spaceboot_slash.c
 *
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

/* Custom exceptions */
extern PyObject * PyExc_ProgramDiffError;

PyObject * slash_csp_switch(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * sipyparam_csh_program(PyObject * self, PyObject * args, PyObject * kwds);
PyObject * slash_sps(PyObject * self, PyObject * args, PyObject * kwds);