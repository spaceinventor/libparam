
#include "pythongetsetarrayparameter.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "../utils.h"
#include "pythongetsetparameter.h"
#include "pythonarrayparameter.h"

PyTypeObject * PythonGetSetArrayParameterType;

// TODO Kevin: Ideally, PythonArrayParameterType.tp_dealloc (and its base classes) would call super, rather than providing a complete implementation themselves.

/* TODO Kevin: Consider if this function could be called with __attribute__((constructor())),
    we just need to ensure Python has been initialized beforehand. */
PyTypeObject * create_pythongetsetarrayparameter_type(void) {

    // Dynamically create the PythonArrayParameter type with multiple inheritance
    PyObject *bases AUTO_DECREF = PyTuple_Pack(2, (PyObject *)&PythonGetSetParameterType, (PyObject *)PythonArrayParameterType);
    if (bases == NULL) {
        return NULL;  
    }
    
    PyObject *dict AUTO_DECREF = PyDict_New();
    if (dict == NULL) {
        return NULL;
    }

    PyObject *name AUTO_DECREF = PyUnicode_FromString("PythonGetSetArrayParameter");
    if (name == NULL) {
        return NULL;  
    }

    PythonGetSetArrayParameterType = (PyTypeObject*)PyObject_CallFunctionObjArgs((PyObject *)&PyType_Type, name, bases, dict, NULL);
    if (PythonGetSetArrayParameterType == NULL) {
        return NULL;  
    }

    return PythonGetSetArrayParameterType;
}
