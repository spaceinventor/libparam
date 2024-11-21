
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "pythonparameter.h"

typedef struct {
    PythonParameterObject python_parameter;
    // No need to include ParameterArrayObject explicitly
    // since it only adds ParameterObject which is already in PythonParameterObject
} PythonArrayParameterObject;

PyTypeObject * create_pythonarrayparameter_type(void);

extern PyTypeObject * PythonArrayParameterType;
