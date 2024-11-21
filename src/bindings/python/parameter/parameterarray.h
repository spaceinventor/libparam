/*
 * parameterarray.h
 *
 * Contains the ParameterArray subclass.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "parameter.h"

typedef struct {
	ParameterObject parameter;
} ParameterArrayObject;

extern PyTypeObject ParameterArrayType;