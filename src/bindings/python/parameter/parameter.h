/*
 * parameter.h
 *
 * Contains the Parameter base class.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <param/param.h>

typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
	PyTypeObject *type;  // Best Python representation of the parameter type, i.e 'int' for uint32.

	param_t * param;

	int host;
	int timeout;
	int retries;  // TODO Kevin: The 'retries' code was implemented rather hastily, consider refactoring of removing it. 
	int paramver;
} ParameterObject;

extern PyTypeObject ParameterType;

/**
 * @brief Takes a param_t and creates and or returns the wrapping ParameterObject.
 * 
 * @param param param_t to find the ParameterObject for.
 * @return ParameterObject* The wrapping ParameterObject.
 */
ParameterObject * ParameterObject_from_param(param_t * param, int host, int timeout, int retries);
