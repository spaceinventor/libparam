
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "pythongetsetparameter.h"

typedef struct {
    PythonGetSetParameterObject python_getset_parameter;
    // No need to include ParameterArrayObject explicitly
    // since it only adds ParameterObject which is already in PythonParameterObject
} PythonGetSetArrayParameterObject;

PyTypeObject * create_pythongetsetarrayparameter_type(void);

extern PyTypeObject * PythonGetSetArrayParameterType;
