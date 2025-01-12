
#include "pythonarrayparameter.h"

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "../utils.h"
#include "pythonparameter.h"
#include "parameterarray.h"

PyTypeObject * PythonArrayParameterType;

// TODO Kevin: Ideally, PythonArrayParameterType.tp_dealloc (and its base classes) would call super, rather than providing a complete implementation themselves.

/* TODO Kevin: Consider if this function could be called with __attribute__((constructor())),
    we just need to ensure Python has been initialized beforehand. */
PyTypeObject * create_pythonarrayparameter_type(void) {

    // Dynamically create the PythonArrayParameter type with multiple inheritance
    PyObject *bases AUTO_DECREF = PyTuple_Pack(2, (PyObject *)&PythonParameterType, (PyObject *)&ParameterArrayType);
    if (bases == NULL) {
        return NULL;  
    }
    
    PyObject *dict AUTO_DECREF = PyDict_New();
    if (dict == NULL) {
        return NULL;
    }

    PyObject *name AUTO_DECREF = PyUnicode_FromString("PythonArrayParameter");
    if (name == NULL) {
        return NULL;  
    }

    PythonArrayParameterType = (PyTypeObject*)PyObject_CallFunctionObjArgs((PyObject *)&PyType_Type, name, bases, dict, NULL);
    if (PythonArrayParameterType == NULL) {
        return NULL;  
    }

    // NOTE Kevin: Allocating PythonArrayParameterType on global scope here, and using memcpy(), seems to cause a segmentation fault when accessing the class.
    // PythonArrayParameterType = *(PyTypeObject *)temp_PythonArrayParameterType;  // Copy class to global, so it can be accessed like the other classes.
    return PythonArrayParameterType;
}
