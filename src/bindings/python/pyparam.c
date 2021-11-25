// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <fcntl.h>
#include <stdio.h>


#include <param/param.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>
#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <pthread.h>
#include <csp/interfaces/csp_if_can.h>
#include <csp/interfaces/csp_if_kiss.h>
#include <csp/interfaces/csp_if_udp.h>
#include <csp/interfaces/csp_if_zmqhub.h>
#include <csp/drivers/usart.h>
#include <csp/drivers/can_socketcan.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_client.h>
#include <param/param.h>
#include <param/param_collector.h>
#include <param/param_queue.h>

#include <sys/types.h>

// TODO Kevin: This is likely not the right way to do this.
// #include "../../../../../src/csp_if_tun.h"
// #include "../../../../../src/csp_if_eth.h"

#include "../../param/param_string.h"


#define PARAMID_CSP_RTABLE					 12

static PyTypeObject ParameterType;

static PyTypeObject ParameterListType;

VMEM_DEFINE_FILE(csp, "csp", "cspcnf.vmem", 120);
VMEM_DEFINE_FILE(params, "param", "params.csv", 50000);
VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
VMEM_DEFINE_FILE(crypto, "crypto", "crypto.csv", 50000);
VMEM_DEFINE_FILE(tfetch, "tfetc", "tfetch.vmem", 120);


PARAM_DEFINE_STATIC_VMEM(PARAMID_CSP_RTABLE,      csp_rtable,        PARAM_TYPE_STRING, 64, 0, PM_SYSCONF, NULL, "", csp, 0, NULL);


static pthread_t router_handle;
void * router_task(void * param) {
	while(1) {
		csp_route_work();
	}
}

static uint8_t _csp_initialized = 0;

param_queue_t param_queue_set = { };
param_queue_t param_queue_get = { };
static int default_node = -1;
static int autosend = 1;
static int paramver = 2;


typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
	PyTypeObject *type;  // Best Python representation of the parameter type, i.e 'int' for uint32.
	//uint32_t mask;

	/* Store Python strings for name and unit, to lessen the overhead of converting them from C */
	PyObject *name;
	PyObject *unit;

	param_t *param;
	char valuebuf[128] __attribute__((aligned(16)));
} ParameterObject;


typedef struct {
	PyListObject list;
	// The documentation warns that for a class to be compatible with multiple inheritance in Python,
	// its .tp_basicsize should be larger than that of its base class; currently not the case here.
	// Source: https://docs.python.org/3/extending/newtypes_tutorial.html#subclassing-other-types
} ParameterListObject;


/* Source: https://pythonextensionpatterns.readthedocs.io/en/latest/super_call.html */
static PyObject * call_super_pyname_lookup(PyObject *self, PyObject *func_name, PyObject *args, PyObject *kwargs) {
    PyObject *result        = NULL;
    PyObject *builtins      = NULL;
    PyObject *super_type    = NULL;
    PyObject *super         = NULL;
    PyObject *super_args    = NULL;
    PyObject *func          = NULL;

    builtins = PyImport_AddModule("builtins");
    if (! builtins) {
        assert(PyErr_Occurred());
        goto except;
    }
    // Borrowed reference
    Py_INCREF(builtins);
    super_type = PyObject_GetAttrString(builtins, "super");
    if (! super_type) {
        assert(PyErr_Occurred());
        goto except;
    }
    super_args = PyTuple_New(2);
    Py_INCREF(self->ob_type);
    if (PyTuple_SetItem(super_args, 0, (PyObject*)self->ob_type)) {
        assert(PyErr_Occurred());
        goto except;
    }
    Py_INCREF(self);
    if (PyTuple_SetItem(super_args, 1, self)) {
        assert(PyErr_Occurred());
        goto except;
    }
    super = PyObject_Call(super_type, super_args, NULL);
    if (! super) {
        assert(PyErr_Occurred());
        goto except;
    }
    func = PyObject_GetAttr(super, func_name);
    if (! func) {
        assert(PyErr_Occurred());
        goto except;
    }
    if (! PyCallable_Check(func)) {
        PyErr_Format(PyExc_AttributeError,
                     "super() attribute \"%S\" is not callable.", func_name);
        goto except;
    }
    result = PyObject_Call(func, args, kwargs);
    assert(! PyErr_Occurred());
    goto finally;
except:
    assert(PyErr_Occurred());
    Py_XDECREF(result);
    result = NULL;
finally:
    Py_XDECREF(builtins);
    Py_XDECREF(super_args);
    Py_XDECREF(super_type);
    Py_XDECREF(super);
    Py_XDECREF(func);
    return result;
}


/* Checks that the argument is a Parameter object, before calling list.append(). */
static PyObject * ParameterList_append(PyObject * self, PyObject * args) {
	PyObject * obj;

	if (!PyArg_ParseTuple(args, "O", &obj)) {
		return NULL;
	}

	if (!PyObject_TypeCheck(obj, &ParameterType)) {
		char errstr[40 + strlen(Py_TYPE(self)->tp_name)];
		sprintf(errstr, "%ss can only contain Parameters.", Py_TYPE(self)->tp_name);
		PyErr_SetString(PyExc_TypeError, errstr);
		return NULL;
	}

	//PyList_Append(self, obj);

	/* 
	Finding the name in the superclass is likely not nearly as efficient 
	as calling list.append() directly. But it is more flexible.
	*/
	PyObject * func_name = Py_BuildValue("s", "append");
	call_super_pyname_lookup(self, func_name, args, NULL);
	Py_DECREF(func_name);

	Py_RETURN_NONE;
}


/* Retrieves a param_t from either its name, id or wrapper object. */
static param_t * pyparam_util_find_param(PyObject * param_identifier, int node) {

	int is_string = PyUnicode_Check(param_identifier);
	int is_int = PyLong_Check(param_identifier);

	param_t * param;

	if (is_string)
		param = param_list_find_name(node, (char*)PyUnicode_AsUTF8(param_identifier));
	else if (is_int)
		param = param_list_find_id(node, (int)PyLong_AsLong(param_identifier));
	else if (PyObject_TypeCheck(param_identifier, &ParameterType))
		param = ((ParameterObject *)param_identifier)->param;
	else {
		PyErr_SetString(PyExc_TypeError,
			"Parameter identifier must be either an integer or string of the parameter ID or name respectively.");
		return NULL;
	}

	return param;
}


/* Gets the best Python representation of the param_t's type, i.e 'int' for 'uint32'.
   Increments the reference count of the found type before returning. */
static PyObject * pyparam_misc_param_type(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	PyObject * param_identifier;
	int node = default_node;

	param_t * param;

	/* Function may be called either as method on 'Paramter object' or standalone function. */
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		ParameterObject *_self = (ParameterObject *)self;

		node = _self->param->node;
		param = _self->param;

	} else {
		if (!PyArg_ParseTuple(args, "O|i", &param_identifier, &node)) {
			return NULL;  // TypeError is thrown
		}

		param = pyparam_util_find_param(param_identifier, node);
	}

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	PyTypeObject * param_type = NULL;

	switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
		case PARAM_TYPE_INT8:
		case PARAM_TYPE_INT16:
		case PARAM_TYPE_INT32:
		case PARAM_TYPE_INT64: {
			param_type = &PyLong_Type;
			break;
		}
		case PARAM_TYPE_FLOAT:
		case PARAM_TYPE_DOUBLE: {
			param_type = &PyFloat_Type;
			break;
		}
		case PARAM_TYPE_STRING: {
			param_type = &PyUnicode_Type;
			break;
		}
		case PARAM_TYPE_DATA: {
			param_type = &PyByteArray_Type;
			break;
		}
		default:  // Raise NotImplementedError when param_type remains NULL.
			break;
	}

	if (param_type == NULL) {
		PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type.");
		return 0;
	}
	Py_INCREF(param_type);
	return (PyObject *)param_type;
}


/* Create a Python Parameter object from a param_t pointer directly. */
static PyObject * _pyparam_Parameter_from_param(PyTypeObject *type, param_t * param) {
	// TODO Kevin: An internal constructor like this is likely bad practice and not very DRY.
	//	Perhaps find a better way?

	ParameterObject *self = (ParameterObject *)type->tp_alloc(type, 0);

	if (self == NULL) {
		return NULL;
	}

	self->param = param;

	self->name = PyUnicode_FromString(param->name);
	if (self->name == NULL) {
		Py_DECREF(self);
		return NULL;
	}
	self->unit = PyUnicode_FromString(param->unit != NULL ? param->unit : "NULL");
	if (self->unit == NULL) {
		Py_DECREF(self);
		return NULL;
	}

	self->type = (PyTypeObject *)pyparam_misc_param_type((PyObject *)self, NULL);

    return (PyObject *) self;
}


/* Constructs a list of Python Parameters of all known param_t returned by param_list_iterate. */
static PyObject * pyparam_util_parameter_list() {

	PyObject * list = PyObject_CallObject((PyObject *)&ParameterListType, NULL);

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		PyObject * parameter = _pyparam_Parameter_from_param(&ParameterType, param);
		PyObject * argtuple = PyTuple_Pack(1, parameter);
		ParameterList_append(list, argtuple);
		Py_DECREF(argtuple);
		Py_DECREF(parameter);
	}

	return list;

}

/* Checks that the specified index is within bounds of the parameter array, raises IndexError if not.
   Supports Python backwards subscriptions. */
static int invalid_index(param_t * arr_param, int *index) {
	if (*index != INT_MIN) {
		if (*index < 0)  // Python backwards subscription.
			*index += arr_param->array_size;
		if (*index < 0 || *index > arr_param->array_size - 1) {
			PyErr_SetString(PyExc_IndexError, "Array Parameter index out of range");
			return -1;
		}
	} else {
		*index = -1;  // Index was not specified, set it to its default value.
	}
	return 0;
}


static PyObject * pyparam_param_get(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int host = -1;
	int node = default_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.

	param_t * param;

	/* Function may be called either as method on 'Paramter object' or standalone function. */
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		ParameterObject *_self = (ParameterObject *)self;

		node = _self->param->node;
		param = _self->param;

		if (args && !PyArg_ParseTuple(args, "|i", &offset))
			return NULL;

	} else {

		static char *kwlist[] = {"param_identifier", "host", "node", "offset", NULL};

		if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iii", kwlist, &param_identifier, &host, &node, &offset))
			return NULL;  // TypeError is thrown

		param = pyparam_util_find_param(param_identifier, node);

	}

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	if (invalid_index(param, &offset))  // Validate the return offset.
		return NULL;  // Raises IndexError.

	/* Remote parameters are sent to a queue or directly */
	int result = 0;
	if (param->node != PARAM_LIST_LOCAL) {

		if (autosend) {
			if (host != -1) {
				result = param_pull_single(param, offset, 1, host, 1000, paramver);
			} else if (node != -1) {
				result = param_pull_single(param, offset, 1, node, 1000, paramver);
			}
		} else {
			if (!param_queue_get.buffer) {
			    param_queue_init(&param_queue_get, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);
			}
			if (param_queue_add(&param_queue_get, param, offset, NULL) < 0)
				printf("Queue full\n");
			param_queue_print(&param_queue_get);
#ifdef PYPARAM_SKIP_CACHED_VAL
			Py_RETURN_NONE;
#endif
		}

	} else if (autosend) {
		param_print(param, -1, NULL, 0, 0);
	} else {
		if (!param_queue_get.buffer) {
			param_queue_init(&param_queue_get, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);
		}
		if (param_queue_add(&param_queue_get, param, offset, NULL) < 0)
			printf("Queue full\n");
		param_queue_print(&param_queue_get);
#ifdef PYPARAM_SKIP_CACHED_VAL
		Py_RETURN_NONE;
#endif
	}

	if (result < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return 0;
	}

	// TODO Kevin: Should be possible to return the entire array.

	switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			if (offset != -1)
				return Py_BuildValue("B", param_get_uint8_array(param, offset));
			return Py_BuildValue("B", param_get_uint8(param));
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			if (offset != -1)
				return Py_BuildValue("H", param_get_uint16_array(param, offset));
			return Py_BuildValue("H", param_get_uint16(param));
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			if (offset != -1)
				return Py_BuildValue("I", param_get_uint32_array(param, offset));
			return Py_BuildValue("I", param_get_uint32(param));
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			if (offset != -1)
				return Py_BuildValue("K", param_get_uint64_array(param, offset));
			return Py_BuildValue("K", param_get_uint64(param));
		case PARAM_TYPE_INT8:
			if (offset != -1)
				return Py_BuildValue("b", param_get_uint8_array(param, offset));
			return Py_BuildValue("b", param_get_uint8(param));
		case PARAM_TYPE_INT16:
			if (offset != -1)
				return Py_BuildValue("h", param_get_uint8_array(param, offset));
			return Py_BuildValue("h", param_get_uint8(param));
		case PARAM_TYPE_INT32:
			if (offset != -1)
				return Py_BuildValue("i", param_get_uint8_array(param, offset));
			return Py_BuildValue("i", param_get_uint8(param));
		case PARAM_TYPE_INT64:
			if (offset != -1)
				return Py_BuildValue("k", param_get_uint8_array(param, offset));
			return Py_BuildValue("k", param_get_uint8(param));
		case PARAM_TYPE_FLOAT:
			if (offset != -1)
				return Py_BuildValue("f", param_get_float_array(param, offset));
			return Py_BuildValue("f", param_get_float(param));
		case PARAM_TYPE_DOUBLE:
			if (offset != -1)
				return Py_BuildValue("d", param_get_double_array(param, offset));
			return Py_BuildValue("d", param_get_double(param));
		case PARAM_TYPE_STRING: {
			char buf[param->array_size];
			param_get_string(param, &buf, param->array_size);
			if (offset != -1) {
				char charstrbuf[] = {buf[offset]};
				return Py_BuildValue("s", charstrbuf);
			}
			return Py_BuildValue("s", buf);
		}
		case PARAM_TYPE_DATA: {
			// TODO Kevin: No idea if this has any chance of working.
			//	I hope it will raise a reasonable exception if it doesn't.
			unsigned int size = (param->array_size > 0) ? param->array_size : 1;
			char buf[size];
			param_get_data(param, buf, size);
			return Py_BuildValue("O&", buf);
		}
	}
	Py_RETURN_NONE;
}


static PyObject * _pyparam_get_str_value(PyObject * obj) {

	if (PyObject_TypeCheck(obj, &ParameterType)) {
		// Return the value of Parameters.
		PyObject * argtuple = PyTuple_Pack(0);
		PyObject * kwdsdict = PyDict_New();
		PyObject * value = pyparam_param_get(obj, argtuple, kwdsdict);
		Py_DECREF(argtuple);
		Py_DECREF(kwdsdict);
		PyObject * strvalue = PyObject_Str(value);
		Py_DECREF(value);
		return strvalue;
	}
	else  // Otherwise use __str__.
		return PyObject_Str(obj);
}


static PyObject * pyparam_param_set(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	PyObject * value;
	int host = -1;
	int node = default_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.

	param_t * param;

	/* Function may be called either as method on 'Paramter object' or standalone function. */
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		/* Parse the value from args */
		if (!PyArg_ParseTuple(args, "O|i", &value, &offset)) {
			return NULL;  // TypeError is thrown
		}

		ParameterObject *_self = (ParameterObject *)self;

		node = _self->param->node;
		param = _self->param;

		PyObject * strvalue = _pyparam_get_str_value(value);

		// Check for dubious value assignments by converting 'strvalue' to '_self.type'.
		// For example: assigning "hello" to an int Parameter.
		PyObject * valuetuple = PyTuple_Pack(1, strvalue);
		PyObject * converted_value = PyObject_CallObject((PyObject *)_self->type, valuetuple);
		if (converted_value == NULL) {
			Py_DECREF(valuetuple);  // converted_value is NULL, so we don't decrements its refcount.
			return NULL;  // We assume failed conversions to have set an exception string.
		}
		Py_DECREF(converted_value);
		Py_DECREF(valuetuple);

		param_str_to_value(param->type, (char*)PyUnicode_AsUTF8(strvalue), _self->valuebuf);
		Py_DECREF(strvalue);

	} else {

		static char *kwlist[] = {"param_identifier", "value", "host", "node", "offset", NULL};
		
		if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|iii", kwlist, &param_identifier, &value, &host, &node, &offset)) {
			return NULL;  // TypeError is thrown
		}

		param = pyparam_util_find_param(param_identifier, node);
	}

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	if (invalid_index(param, &offset))  // Validate the return offset.
		return NULL;  // Raises IndexError.

	char valuebuf[128] __attribute__((aligned(16))) = { };
	PyObject * strvalue = _pyparam_get_str_value(value);
	param_str_to_value(param->type, (char*)PyUnicode_AsUTF8(strvalue), valuebuf);
	Py_DECREF(strvalue);

	/* Remote parameters are sent to a queue or directly */
	int result = 0;
	if (param->node != PARAM_LIST_LOCAL) {

		if ((node != -1) && (autosend)) {
			result = param_push_single(param, offset, valuebuf, 1, node, 1000, paramver);
		} else if (host != -1) {
			result = param_push_single(param, offset, valuebuf, 1, host, 1000, paramver);
		} else {
			if (!param_queue_set.buffer) {
				param_queue_init(&param_queue_set, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
			}
			if (param_queue_add(&param_queue_set, param, offset, valuebuf) < 0)
				printf("Queue full\n");
			param_queue_print(&param_queue_set);
			Py_RETURN_NONE;
		}
		
	} else if (autosend) {
		/* For local parameters, set immediately if autosend is enabled */
	    if (offset < 0)
			offset = 0;
		param_set(param, offset, valuebuf);
	} else {
		/* If autosend is off, queue the parameters */
		if (!param_queue_set.buffer) {
			param_queue_init(&param_queue_set, malloc(PARAM_SERVER_MTU), PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
		}
		if (param_queue_add(&param_queue_set, param, offset, valuebuf) < 0)
			printf("Queue full\n");
		param_queue_print(&param_queue_set);
		Py_RETURN_NONE;
	}

	if (result < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	}

	param_print(param, -1, NULL, 0, 2);

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_pull(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int host = 0;
	uint32_t include_mask = 0xFFFFFFFF;
	uint32_t exclude_mask = PM_REMOTE | PM_HWREG;
	unsigned int timeout = 1000;

	char * _str_include_mask;
	char * _str_exclude_mask;

	static char *kwlist[] = {"host", "include_mask", "exclude_mask", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|ssI", kwlist, &host, &_str_include_mask, &_str_exclude_mask, &timeout)) {
		return NULL;
	}

	if (_str_include_mask != NULL)
		include_mask = param_maskstr_to_mask(_str_include_mask);
	if (_str_exclude_mask != NULL)
		exclude_mask = param_maskstr_to_mask(_str_exclude_mask);

	int result = -1;
	if (param_queue_get.used == 0) {
		result = param_pull_all(1, host, include_mask, exclude_mask, timeout, paramver);
	} else {
		result = param_pull_queue(&param_queue_get, 1, host, timeout);
	}

	if (result) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_push(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int node = 0;
	unsigned int timeout = 100;

	if (!PyArg_ParseTuple(args, "I|I", &node, &timeout)) {
		return NULL;  // TypeError is thrown
	}

	if (param_push_queue(&param_queue_set, 1, node, timeout) < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_clear(PyObject * self, PyObject * args) {

	param_queue_get.used = 0;
	param_queue_set.used = 0;
    param_queue_get.version = paramver;
    param_queue_set.version = paramver;
	printf("Queue cleared\n");
	Py_RETURN_NONE;

}

static PyObject * pyparam_param_node(PyObject * self, PyObject * args) {

	int node = -1;

	if (!PyArg_ParseTuple(args, "|i", &node)) {
		return NULL;  // TypeError is thrown
	}

	if (node == -1)
		printf("Default node = %d\n", default_node);
	else {
		default_node = node;
		printf("Set default node to %d\n", default_node);
	}

	return Py_BuildValue("i", default_node);
}

static PyObject * pyparam_param_paramver(PyObject * self, PyObject * args) {

	// Not sure if the static paramver would be set to NULL, when nothing is passed.
	int _paramver = -1;  

	if (!PyArg_ParseTuple(args, "|i", &_paramver)) {
		return NULL;  // TypeError is thrown
	}

	if (_paramver == -1)
		printf("Parameter client version = %d\n", paramver);
	else {
		paramver = _paramver;
		printf("Set parameter client version to %d\n", paramver);
	}

	return Py_BuildValue("i", paramver);
}

static PyObject * pyparam_param_autosend(PyObject * self, PyObject * args) {

	int _autosend = -1;

	if (!PyArg_ParseTuple(args, "|i", &_autosend)) {
		return NULL;  // TypeError is thrown
	}

	if (_autosend == -1)
		printf("autosend = %d\n", autosend);
	else {
		autosend = _autosend;
		printf("Set autosend to %d\n", autosend);
	}

	return Py_BuildValue("i", autosend);
}

static PyObject * pyparam_param_queue(PyObject * self, PyObject * args) {

	if ( (param_queue_get.used == 0) && (param_queue_set.used == 0) ) {
		printf("Nothing queued\n");
	}
	if (param_queue_get.used > 0) {
		printf("Get Queue\n");
		param_queue_print(&param_queue_get);
	}
	if (param_queue_set.used > 0) {
		printf("Set Queue\n");
		param_queue_print(&param_queue_set);
	}

	Py_RETURN_NONE;

}


static PyObject * pyparam_param_list(PyObject * self, PyObject * args) {

	uint32_t mask = 0xFFFFFFFF;

	char * _str_mask = NULL;

	if (!PyArg_ParseTuple(args, "|s", &_str_mask)) {
		return NULL;
	}

	if (_str_mask != NULL)
		mask = param_maskstr_to_mask(_str_mask);

	param_list_print(mask);

	return pyparam_util_parameter_list();
}

static PyObject * pyparam_param_list_download(PyObject * self, PyObject * args, PyObject * kwds) {

	unsigned int node;
    unsigned int timeout = 1000;
    unsigned int version = 2;

	static char *kwlist[] = {"node", "timeout", "version", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|II", kwlist, &node, &timeout, &version)) {
		return NULL;  // TypeError is thrown
	}

	param_list_download(node, timeout, version);

	return pyparam_util_parameter_list();

}


static PyObject * pyparam_csp_ping(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int node;
	unsigned int timeout = 1000;
	unsigned int size = 1;

	static char *kwlist[] = {"node", "timeout", "size", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|II", kwlist, &node, &timeout, &size)) {
		return NULL;  // TypeError is thrown
	}

	printf("Ping node %u size %u timeout %u: ", node, size, timeout);

	int result = csp_ping(node, timeout, size, CSP_O_CRC32);

	if (result >= 0) {
		printf("Reply in %d [ms]\n", result);
	} else {
		printf("No reply\n");
	}

	return Py_BuildValue("i", result);

}

static PyObject * pyparam_csp_ident(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int node;
	unsigned int timeout = 1000;

	static char *kwlist[] = {"node", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|I", kwlist, &node, &timeout)) {
		return NULL;  // TypeError is thrown
	}

	struct csp_cmp_message message;

	if (csp_cmp_ident(node, timeout, &message) != CSP_ERR_NONE) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return 0;
	}

	printf("%s\n%s\n%s\n%s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);

	Py_RETURN_NONE;
	
}


static void Parameter_dealloc(ParameterObject *self) {
	Py_XDECREF((PyObject*)self->type);
	Py_XDECREF(self->name);
	Py_XDECREF(self->unit);
	// Get the type of 'self' in case the user has subclassed 'Parameter'.
	// Not that this makes a lot of sense to do.
	Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject * Parameter_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	ParameterObject *self = (ParameterObject *) type->tp_alloc(type, 0);

	if (self == NULL)
		return NULL;

	static char *kwlist[] = {"param_identifier", "node", NULL};

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	//int host = -1;
	int node = default_node;
	//int offset = -1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|H", kwlist, &param_identifier, &node)) {
		return NULL;  // TypeError is thrown
	}

	param_t * param = pyparam_util_find_param(param_identifier, node);

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	self->param = param;

	self->name = PyUnicode_FromString(param->name);
	if (self->name == NULL) {
		Py_DECREF(self);
		return NULL;
	}
	self->unit = PyUnicode_FromString(param->unit != NULL ? param->unit : "NULL");
	if (self->unit == NULL) {
		Py_DECREF(self);
		return NULL;
	}

	self->type = (PyTypeObject *)pyparam_misc_param_type((PyObject *)self, NULL);

    return (PyObject *) self;
}

static PyObject * Parameter_getname(ParameterObject *self, void *closure) {
	Py_INCREF(self->name);
	return self->name;
}

static PyObject * Parameter_getunit(ParameterObject *self, void *closure) {
	Py_INCREF(self->unit);
	return self->unit;
}

static PyObject * Parameter_getid(ParameterObject *self, void *closure) {
	return Py_BuildValue("H", self->param->id);
}

static PyObject * Parameter_getnode(ParameterObject *self, void *closure) {
	return Py_BuildValue("H", self->param->node);
}

/* This will change self->param to be one by the same name at the specified node. */
static int Parameter_setnode(ParameterObject *self, PyObject *value, void *closure) {
	// It seems that exceptions created in C setters aren't raised immediately after return,
	// they are usually raised on the following expression in Python instead.

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the node attribute");
        return -1;
    }

	if(!PyLong_Check(value)) {
		PyErr_SetString(PyExc_TypeError,
                        "The node attribute must be set to an int");
        return -1;
	}

	uint16_t node;

	// This is pretty stupid, but seems to be the easiest way to convert a long to short using Python.
	PyObject * value_tuple = PyTuple_Pack(1, value);
	if (!PyArg_ParseTuple(value_tuple, "H", &node)) {
		Py_DECREF(value_tuple);
		return -1;
	}
	Py_DECREF(value_tuple);

	param_t * param = pyparam_util_find_param(self->name, node);

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	self->param = param;

	return 0;
}

static PyObject * Parameter_gettype(ParameterObject *self, void *closure) {
	Py_INCREF(self->type);
	return (PyObject *)self->type;
}

static PyObject * Parameter_getvalue(ParameterObject *self, void *closure) {
	return pyparam_param_get((PyObject *)self, NULL, NULL);
}

static int Parameter_setvalue(ParameterObject *self, PyObject *value, void *closure) {
	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the value attribute");
        return -1;
    }
	PyObject * value_tuple = PyTuple_Pack(1, value);
	pyparam_param_set((PyObject *)self, value_tuple, NULL);
	Py_DECREF(value_tuple);
	return 0;
}

static PyObject * Parameter_str(ParameterObject *self) {
	char buf[100];
	sprintf(buf, "[id:%i|node:%i] %s | %s", self->param->id, self->param->node, self->param->name, self->type->tp_name);
	return Py_BuildValue("s", buf);
}

static PyObject * Parameter_GetItem(ParameterObject *self, PyObject *item) {
	PyObject * valuetuple = PyTuple_Pack(1, item);
	PyObject * returnvalue = pyparam_param_get((PyObject *)self, valuetuple, NULL);
	Py_DECREF(valuetuple);
	return returnvalue;
}

static int Parameter_SetItem(ParameterObject *self, PyObject* item, PyObject* value) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete parameter array indexes.");
        return -1;
    }
	PyObject * value_tuple = PyTuple_Pack(2, value, item);  // value, index
	if (!pyparam_param_set((PyObject *)self, value_tuple, NULL)) {
		Py_DECREF(value_tuple);
		return -1;
	}
	Py_DECREF(value_tuple);
	return 0;
}

static Py_ssize_t Parameter_length(ParameterObject *self) {
	// We currently raise an exception when getting len() of non-array type parameters.
	// This stops PyCharm (Perhaps other IDE's) from showing their length as 0. ¯\_(ツ)_/¯
	if (!self->param->array_size)
		PyErr_SetString(PyExc_AttributeError, "Non-array type parameter is not subscriptable");
	return self->param->array_size;
}

static PyMappingMethods Parameter_as_mapping = {
    (lenfunc)Parameter_length,
    (binaryfunc)Parameter_GetItem,
    (objobjargproc)Parameter_SetItem
};

/* 
The Python binding 'Parameter' class exposes most of its attributes through getters, 
as only its 'value' and 'node' are mutable, and even those are through setters.
*/
static PyGetSetDef Parameter_getsetters[] = {
    {"name", (getter) Parameter_getname, NULL,
     "name of the parameter", NULL},
    {"unit", (getter) Parameter_getunit, NULL,
     "unit of the parameter", NULL},
	{"id", (getter) Parameter_getid, NULL,
     "id of the parameter", NULL},
	{"node", (getter) Parameter_getnode, (setter)Parameter_setnode,
     "node of the parameter", NULL},
	{"type", (getter) Parameter_gettype, NULL,
     "type of the parameter", NULL},
	{"value", (getter) Parameter_getvalue, (setter)Parameter_setvalue,
     "value of the parameter", NULL},
    {NULL}  /* Sentinel */
};

static PyTypeObject ParameterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libparam_py3.Parameter",
    .tp_doc = "Wrapper utility class for libparam parameters.",
    .tp_basicsize = sizeof(ParameterObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = Parameter_new,
    .tp_dealloc = (destructor)Parameter_dealloc,
	.tp_getset = Parameter_getsetters,
	.tp_str = (reprfunc)Parameter_str,
	.tp_as_mapping = &Parameter_as_mapping,
};


// static void ParameterList_dealloc(ParameterListObject *self) {
// 	// Get the type of 'self' in case the user has subclassed 'Parameter'.
// 	// Not that this makes a lot of sense to do.
// 	Py_TYPE(self)->tp_free((PyObject *) self);
// }

/* Pulls all Parameters in the list as a single request. */
static PyObject * ParameterList_pull(ParameterListObject *self, PyObject *args, PyObject *kwds) {
	
	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	unsigned int host = 0;
	unsigned int timeout = 100;

	if (!PyArg_ParseTuple(args, "I|I", &host, &timeout)) {
		return NULL;  // TypeError is thrown
	}

	void * queuebuffer = malloc(PARAM_SERVER_MTU);
	param_queue_t queue = { };
	param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);

	int seqlen = PySequence_Fast_GET_SIZE(self);

	for (int i = 0; i < seqlen; i++) {

		PyObject *item = PySequence_Fast_GET_ITEM(self, i);

		if(!item) {
            Py_DECREF(args);
			free(queuebuffer);
			PyErr_SetString(PyExc_RuntimeError, "Iterator went outside the bounds of the list.");
            return NULL;
        }

		if (PyObject_TypeCheck(item, &ParameterType))  // Sanity check
			param_queue_add(&queue, ((ParameterObject *)item)->param, -1, NULL);
		else
			fprintf(stderr, "Skipping non-parameter object (of type: %s) in Parameter list.", item->ob_type->tp_name);

	}

	if (param_pull_queue(&queue, 0, host, timeout)) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		free(queuebuffer);
		return 0;
	}

	free(queuebuffer);
	Py_RETURN_NONE;
}

/* Pushes all Parameters in the list as a single request. */
static PyObject * ParameterList_push(ParameterListObject *self, PyObject *args, PyObject *kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}
	
	unsigned int node = 0;
	unsigned int timeout = 100;

	if (!PyArg_ParseTuple(args, "I|I", &node, &timeout)) {
		return NULL;  // TypeError is thrown
	}

	void * queuebuffer = malloc(PARAM_SERVER_MTU);
	param_queue_t queue = { };
	param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);

	int seqlen = PySequence_Fast_GET_SIZE(self);

	for (int i = 0; i < seqlen; i++) {

		PyObject *item = PySequence_Fast_GET_ITEM(self, i);

		if(!item) {
            Py_DECREF(args);
			free(queuebuffer);
			PyErr_SetString(PyExc_RuntimeError, "Iterator went outside the bounds of the list.");
            return NULL;
        }

		if (PyObject_TypeCheck(item, &ParameterType))  // Sanity check
			param_queue_add(&queue, ((ParameterObject *)item)->param, -1, ((ParameterObject *)item)->valuebuf);
		else
			fprintf(stderr, "Skipping non-parameter object (of type: %s) in Parameter list.", item->ob_type->tp_name);
	}

	if (param_push_queue(&queue, 1, node, timeout) < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		free(queuebuffer);
		return NULL;
	}

	free(queuebuffer);
	Py_RETURN_NONE;
}

static PyMethodDef ParameterList_methods[] = {
    {"append", (PyCFunction) ParameterList_append, METH_VARARGS,
     PyDoc_STR("Add a Parameter to the list.")},
	{"pull", (PyCFunction) ParameterList_pull, METH_VARARGS,
     PyDoc_STR("Pulls all Parameters in the list as a single request.")},
	{"push", (PyCFunction) ParameterList_push, METH_VARARGS,
     PyDoc_STR("Pushes all Parameters in the list as a single request.")},
    {NULL},
};

static int ParameterList_init(ParameterListObject *self, PyObject *args, PyObject *kwds) {

	int seqlen = PySequence_Fast_GET_SIZE(args);

	// This .__init__() function accepts either an iterable or (Python) *args as its initial members.
	if (seqlen == 1 && PySequence_Check(PySequence_Fast_GET_ITEM(args, 0)))
		// This likely fails to have its intended effect if args is some weird nested iterable like:
		// (((((1)), 2))), but this initializes a ParameterList incorrectly anyway. 
		return ParameterList_init(self, PySequence_Fast_GET_ITEM(args, 0), kwds);

	if (PyList_Type.tp_init((PyObject *) self, PyTuple_Pack(0), kwds) < 0)
        return -1;
        
	/* We append all the items passed as arguments to self. Checking that they are Parameters as we go. */
	/* Iterate over the objects in args directly, as opposed to copying them first.
		I'm not sure this is entirely legal, considering the GIL and multiprocessing stuff. */
	for (int i = 0; i < seqlen; i++) {

		PyObject *item = PySequence_Fast_GET_ITEM(args, i);

		if(!item) {
            Py_DECREF(args);
			PyErr_SetString(PyExc_RuntimeError, "Iterator went outside the bounds of the list.");
            return -1;
        }

		PyObject * valuetuple = PyTuple_Pack(1, item);
		ParameterList_append((PyObject *)self, valuetuple);
		Py_DECREF(valuetuple);
		if (PyErr_Occurred())
			return -1;

	}

    return 0;
}

/* Subclass of the Python list which implements an interface to libparam's queue API. 
   This makes some attempts to restricts and validate its contents to be parameters.
   This is generally considered unpythonic however, and should't be relied upon */
static PyTypeObject ParameterListType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libparam_py3.ParameterList",
	.tp_doc = "Parameter list class with an interface to libparam's queue API.",
	.tp_basicsize = sizeof(ParameterListObject),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_init = (initproc)ParameterList_init,
	// .tp_dealloc = (destructor)ParameterList_dealloc,
	.tp_methods = ParameterList_methods,
};


void * param_collector_task(void * param) {
	param_collector_loop(param);
	return NULL;
}

void * vmem_server_task(void * param) {
	vmem_server_loop(param);
	return NULL;
}


static PyObject * pyparam_init(PyObject * self, PyObject * args, PyObject *kwds) {

	if (_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot initialize multiple instances of libparam bindings. Please use a previous binding.");
			return 0;
	}

	static char *kwlist[] = {
		"csp_address", "csp_version", "csp_hostname", "csp_model", "csp_port", 
		"uart_dev", "uart_baud", "can_dev", "udp_peer_str", "udp_peer_idx", "tun_conf_str", "eth_ifname", 
		"csp_zmqhub_addr", "csp_zmqhub_idx", "quiet", NULL
	};

	csp_conf.address = 1;
	csp_conf.version = 2;
	csp_conf.hostname = "python_bindings";
	csp_conf.model = "linux";

	uint16_t csp_port = PARAM_PORT_SERVER;

	char *uart_dev = NULL;  // "/dev/ttyUSB0"
	uint32_t uart_baud = 1000000;

	char * can_dev = NULL;  // can0
	char * udp_peer_str[10];  // TODO Kevin: Not sure what effect declaring the size of the array will have when Python likely alters the pointer address.
	int udp_peer_idx = 0;
	char * tun_conf_str = NULL;
	char * eth_ifname = NULL;
	char * csp_zmqhub_addr[10];
	int csp_zmqhub_idx = 0;
	
	int quiet = 0;
	

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|HBssHsIssisssii", kwlist, 
		&csp_conf.address, &csp_conf.version,  &csp_conf.hostname, &csp_conf.model, 
		&csp_port, &uart_dev, &uart_baud, &can_dev, &udp_peer_str, &udp_peer_idx, 
		&tun_conf_str, &eth_ifname, &csp_zmqhub_addr, &csp_zmqhub_idx, &quiet)
	)
		return NULL;  // TypeError is thrown

	if (quiet) {
		static FILE * devnull;
		if ((devnull = fopen("/dev/null", "w")) == NULL) {
			fprintf(stderr, "Impossible error! Can't open /dev/null: %s\n", strerror(errno));
			exit(1);
		}
		stdout = devnull;
	}

	/* Get csp config from file */
	vmem_file_init(&vmem_csp);

	/* Parameters */
	vmem_file_init(&vmem_params);
	param_list_store_vmem_load(&vmem_params);

	
	csp_init();

	csp_iface_t * default_iface = NULL;
	if (uart_dev != NULL) {
		csp_usart_conf_t conf = {
			.device = uart_dev,
			.baudrate = uart_baud, /* supported on all platforms */
			.databits = 8,
			.stopbits = 1,
			.paritysetting = 0,
			.checkparity = 0
		};
		int error = csp_usart_open_and_add_kiss_interface(&conf, CSP_IF_KISS_DEFAULT_NAME, &default_iface);
		if (error != CSP_ERR_NONE) {
			csp_log_error("failed to add KISS interface [%s], error: %d", uart_dev, error);
			exit(1);
		}
	}

	if (can_dev != NULL) {
		int error = csp_can_socketcan_open_and_add_interface(can_dev, CSP_IF_CAN_DEFAULT_NAME, 1000000, true, &default_iface);
		if (error != CSP_ERR_NONE) {
			csp_log_error("failed to add CAN interface [%s], error: %d", can_dev, error);
		}
	}


	pthread_create(&router_handle, NULL, &router_task, NULL);

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);


	while (udp_peer_idx > 0) {
		char * udp_str = udp_peer_str[--udp_peer_idx];
		printf("udp str %s\n", udp_str);

		int lport = 9600;
		int rport = 9600;
		char udp_peer_ip[20];

		if (sscanf(udp_str, "%d %19s %d", &lport, udp_peer_ip, &rport) != 3) {
			printf("Invalid UDP configuration string: %s\n", udp_str);
			printf("Should math the pattern \"<lport> <peer ip> <rport>\" exactly\n");
			return NULL;
		}

		csp_iface_t * udp_client_if = malloc(sizeof(csp_iface_t));
		csp_if_udp_conf_t * udp_conf = malloc(sizeof(csp_if_udp_conf_t));
		udp_conf->host = udp_peer_ip;
		udp_conf->lport = lport;
		udp_conf->rport = rport;
		csp_if_udp_init(udp_client_if, udp_conf);

		/* Use auto incrementing names */
		char * udp_name = malloc(20);
		sprintf(udp_name, "UDP%u", udp_peer_idx);
		udp_client_if->name = udp_name;

		default_iface = udp_client_if;
	}

	if (tun_conf_str) {

		int src;
		int dst;

		if (sscanf(tun_conf_str, "%d %d", &src, &dst) != 2) {
			printf("Invalid TUN configuration string: %s\n", tun_conf_str);
			printf("Should math the pattern \"<src> <dst>\" exactly\n");
			return NULL;
		}

		// csp_iface_t * tun_if = malloc(sizeof(csp_iface_t));
		// csp_if_tun_conf_t * ifconf = malloc(sizeof(csp_if_tun_conf_t));

		// ifconf->tun_dst = dst;
		// ifconf->tun_src = src;

		// csp_if_tun_init(tun_if, ifconf);

	}

	// if (eth_ifname) {
	// 	static csp_iface_t csp_iface_eth;
	// 	csp_if_eth_init(&csp_iface_eth, eth_ifname);
	// 	default_iface = &csp_iface_eth;
	// }

	while (csp_zmqhub_idx > 0) {
		char * zmq_str = csp_zmqhub_addr[--csp_zmqhub_idx];
		printf("zmq str %s\n", zmq_str);
		csp_iface_t * zmq_if;
		csp_zmqhub_init(csp_get_address(), zmq_str, 0, &zmq_if);

		/* Use auto incrementing names */
		char * zmq_name = malloc(20);
		sprintf(zmq_name, "ZMQ%u", csp_zmqhub_idx);
		zmq_if->name = zmq_name;

		default_iface = zmq_if;
	}


	char saved_rtable[csp_rtable.array_size];
	char * rtable = NULL;


	if (!rtable) {
		/* Read routing table from parameter system */
		param_get_string(&csp_rtable, saved_rtable, csp_rtable.array_size);
		rtable = saved_rtable;
	}

	if (csp_rtable_check(rtable)) {
		int error = csp_rtable_load(rtable);
		if (error < 1) {
			csp_log_error("csp_rtable_load(%s) failed, error: %d", rtable, error);
			//exit(1);
		}
	} else if (default_iface) {
		printf("Setting default route to %s\n", default_iface->name);
		csp_rtable_set(0, 0, default_iface, CSP_NO_VIA_ADDRESS);
	} else {
		printf("No routing defined\n");
	}

	csp_socket_t *sock_csh = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_csh, csp_service_handler);
	csp_bind(sock_csh, CSP_ANY);

	csp_socket_t *sock_param = csp_socket(CSP_SO_NONE);
	csp_socket_set_callback(sock_param, param_serve);
	csp_bind(sock_param, csp_port);

	pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

	/* Start a collector task */
	vmem_file_init(&vmem_col);

	pthread_t param_collector_handle;
	pthread_create(&param_collector_handle, NULL, &param_collector_task, NULL);

	/* Crypto magic */
	vmem_file_init(&vmem_crypto);
	// crypto_key_refresh();

	/* Test of time fetch */
	vmem_file_init(&vmem_tfetch);
	// tfetch_onehz();
	
	_csp_initialized = 1;
	Py_RETURN_NONE;

}

static PyMethodDef methods[] = {

	/* Converted Slash/Satctl commands from param/param_slash.c */
	{"set", 		(PyCFunction)pyparam_param_set, METH_VARARGS | METH_KEYWORDS, 	""},
	{"get", 		(PyCFunction)pyparam_param_get, METH_VARARGS | METH_KEYWORDS, 	""},
	{"push", 		pyparam_param_push, 			METH_VARARGS, 					""},
	{"pull", 		(PyCFunction)pyparam_param_pull, METH_VARARGS | METH_KEYWORDS, 	""},
	{"clear", 		pyparam_param_clear, 			METH_NOARGS, 					""},
	{"node", 		pyparam_param_node, 			METH_VARARGS, 					""},
	{"paramver", 	pyparam_param_paramver, 		METH_VARARGS, 					""},
	{"autosend", 	pyparam_param_autosend, 		METH_VARARGS, 					""},
	{"queue", 		pyparam_param_queue, 			METH_NOARGS, 					""},

	/* Converted Slash/Satctl commands from param/param_list_slash.c */
	{"list", 		pyparam_param_list, 			METH_VARARGS, 					""},
	{"list_download", (PyCFunction)pyparam_param_list_download, METH_VARARGS | METH_KEYWORDS, ""},

	/* Converted Slash/Satctl commands from slash_csp.c */
	/* Including these here is not entirely optimal, they may be removed. */
	{"ping", 		(PyCFunction)pyparam_csp_ping, 	METH_VARARGS | METH_KEYWORDS, 	""},
	{"ident", 		(PyCFunction)pyparam_csp_ident, METH_VARARGS | METH_KEYWORDS, 	""},

	/* Miscellaneous utility functions */
	{"get_type", 	pyparam_misc_param_type, 		METH_VARARGS, 					""},

	/* Misc */
	{"_param_init", (PyCFunction)pyparam_init, 	METH_VARARGS | METH_KEYWORDS, 	""},

	/* sentinel */
	{NULL, NULL, 0, NULL}};

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"libparam_py3",
	"Module ",
	-1,
	methods,
	NULL,
	NULL,
	NULL,
	NULL};

PyMODINIT_FUNC PyInit_libparam_py3(void) {

	if (PyType_Ready(&ParameterType) < 0)
        return NULL;

	ParameterListType.tp_base = &PyList_Type;
	if (PyType_Ready(&ParameterListType) < 0)
		return NULL;

	PyObject * m = PyModule_Create(&moduledef);
	if (m == NULL)
		return NULL;

	Py_INCREF(&ParameterType);
	if (PyModule_AddObject(m, "Parameter", (PyObject *) &ParameterType) < 0) {
		Py_DECREF(&ParameterType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(&ParameterListType);
	if (PyModule_AddObject(m, "ParameterList", (PyObject *)&ParameterListType) < 0) {
		Py_DECREF(&ParameterListType);
		Py_DECREF(m);
		return NULL;
	}

	return m;
}
