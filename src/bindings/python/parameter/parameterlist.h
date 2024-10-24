/*
 * parameterlist.h
 *
 * Contains the ParameterList class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

typedef struct {
	PyListObject list;
	// The documentation warns that for a class to be compatible with multiple inheritance in Python,
	// its .tp_basicsize should be larger than that of its base class; currently not the case here.
	// Source: https://docs.python.org/3/extending/newtypes_tutorial.html#subclassing-other-types
} ParameterListObject;

PyObject * ParameterList_append(PyObject * self, PyObject * args);

extern PyTypeObject ParameterListType;