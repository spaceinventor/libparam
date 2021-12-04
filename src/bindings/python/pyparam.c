// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <fcntl.h>
#include <stdio.h>


#include <param/param.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_client.h>
#include <vmem/vmem_ram.h>
#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/csp_yaml.h>
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


uint8_t test_array[] = {1,2,3,4,5,6,7,8};
PARAM_DEFINE_STATIC_RAM(10001, test_array_param,          PARAM_TYPE_UINT8,  8, sizeof(uint8_t),  PM_DEBUG, NULL, "", test_array, NULL);


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


/* Retrieves a param_t from either its name, id or wrapper object.
   May raise TypeError or ValueError, returned value will be NULL in either case. */
static param_t * _pyparam_util_find_param_t(PyObject * param_identifier, int node) {

	param_t * param = NULL;

	if (PyUnicode_Check(param_identifier))  // is_string
		param = param_list_find_name(node, (char*)PyUnicode_AsUTF8(param_identifier));
	else if (PyLong_Check(param_identifier))  // is_int
		param = param_list_find_id(node, (int)PyLong_AsLong(param_identifier));
	else if (PyObject_TypeCheck(param_identifier, &ParameterType))
		param = ((ParameterObject *)param_identifier)->param;
	else {  // Invalid type passed.
		PyErr_SetString(PyExc_TypeError,
			"Parameter identifier must be either an integer or string of the parameter ID or name respectively.");
		return NULL;
	}

	if (param == NULL)  // Check if a parameter was found.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");

	return param;  // or NULL for ValueError.
}


/* Gets the best Python representation of the param_t's type, i.e 'int' for 'uint32'.
   Does not increment the reference count of the found type before returning.
   May raise TypeError for unsupported parameter types (none exist at time of writing). */
static PyTypeObject * _pyparam_misc_param_t_type(param_t * param) {

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

	if (param_type == NULL)
		PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type.");

	return param_type;  // or NULL (for NotImplementedError).
}


/* Public interface for '_pyparam_misc_param_t_type()' 
   Increments the reference count of the found type before returning. */
static PyObject * pyparam_misc_get_type(PyObject * self, PyObject * args) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;
	int node = default_node;

	param_t * param;

	/* Function may be called either as method on 'Parameter' object or standalone function. */
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		ParameterObject *_self = (ParameterObject *)self;

		node = _self->param->node;
		param = _self->param;

	} else {
		if (!PyArg_ParseTuple(args, "O|i", &param_identifier, &node)) {
			return NULL;  // TypeError is thrown
		}

		param = _pyparam_util_find_param_t(param_identifier, node);
	}

	if (param == NULL) {  // Did not find a match.
		return NULL;  // Raises either TypeError or ValueError.
	}

	PyTypeObject * result = _pyparam_misc_param_t_type(param);
	Py_INCREF(result);

	return (PyObject *)result;
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

	self->type = (PyTypeObject *)pyparam_misc_get_type((PyObject *)self, NULL);

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

/* Checks that the specified index is within bounds of the sequence index, raises IndexError if not.
   Supports Python backwards subscriptions, mutates the index to a positive value in such cases. */
static int _pyparam_util_index(int seqlen, int *index) {
	if (*index < 0)  // Python backwards subscription.
		*index += seqlen;
	if (*index < 0 || *index > seqlen - 1) {
		PyErr_SetString(PyExc_IndexError, "Array Parameter index out of range");
		return -1;
	}

	return 0;
}

/* Private interface for getting the value of single parameter
   Increases the reference count of the returned item before returning. 
   Use INT_MIN for offset as no offset. */
static PyObject * _pyparam_util_get_single(param_t *param, int offset, int autopull) {

	if (offset != INT_MIN) {
		if (_pyparam_util_index(param->array_size, &offset))  // Validate the offset.
			return NULL;  // Raises IndexError.
	} else
		offset = -1;

	if (autopull && (param->node != PARAM_LIST_LOCAL)) {

		if (param_pull_single(param, offset, 1, param->node, 1000, paramver)) {
			PyErr_SetString(PyExc_ConnectionError, "No response");
			return NULL;
		}
	}

	param_print(param, -1, NULL, 0, 0);

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
			//	I hope it will raise a reasonable exception if it doesn't,
			//	instead of just segfaulting :P
			unsigned int size = (param->array_size > 0) ? param->array_size : 1;
			char buf[size];
			param_get_data(param, buf, size);
			return Py_BuildValue("O&", buf);
		}
	}
	PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type for get operation.");
	return NULL;
}

static PyObject * _pyparam_util_get_array(param_t *param, int autopull) {

	// Pull the value for each index (if we're allowed to),
	// instead of pulling them individually.
	if (autopull && param->node != PARAM_LIST_LOCAL) {
		void * queuebuffer = malloc(PARAM_SERVER_MTU);
		param_queue_t queue = { };
		param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);

		for (int i = 0; i < param->array_size; i++) {
			param_queue_add(&queue, param, i, NULL);
		}

		if (param_pull_queue(&queue, 0, param->node, 2000)) {
			PyErr_SetString(PyExc_ConnectionError, "No response.");
			free(queuebuffer);
			return 0;
		}

		free(queuebuffer);
	}
	
	// We will populate this tuple with the values from the indexes.
	PyObject * value_tuple = PyTuple_New(param->array_size);

	for (int i = 0; i < param->array_size; i++) {
		PyObject * item = _pyparam_util_get_single(param, i, 0);

		if (item == NULL) {  // Something went wrong, probably a connectionerror. Let's abandon ship.
			Py_DECREF(value_tuple);
			return NULL;
		}
		
		PyTuple_SET_ITEM(value_tuple, i, item);
	}
	
	return value_tuple;
}


static PyObject * pyparam_param_get(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int host = -1;
	int node = default_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.

	static char *kwlist[] = {"param_identifier", "host", "node", "offset", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iii", kwlist, &param_identifier, &host, &node, &offset))
		return NULL;  // TypeError is thrown

	param_t *param = _pyparam_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	// _pyparam_util_get_single() and _pyparam_util_get_array() will return NULL for exceptions, which is fine with us.
	if (param->array_size > 0)
		return _pyparam_util_get_array(param, autosend);
	return _pyparam_util_get_single(param, offset, autosend);
}


static PyObject * _pyparam_get_str_value(PyObject * obj) {

	// This 'if' exists for cases where the value 
	// of a parmeter is assigned from that of another.
	// i.e: 				param1.value = param2
	// Which is equal to:	param1.value = param2.value
	if (PyObject_TypeCheck(obj, &ParameterType)) {
		// Return the value of the Parameter.

		param_t * param = ((ParameterObject *)obj)->param;

		PyObject * value = param->array_size > 0 ? 
			_pyparam_util_get_array(param, autosend) :
			_pyparam_util_get_single(param, INT_MIN, autosend);

		PyObject * strvalue = PyObject_Str(value);
		Py_DECREF(value);
		return strvalue;
	}
	else  // Otherwise use __str__.
		return PyObject_Str(obj);
}

/* Attempts a conversion to the specified type, by calling it. */
static PyObject * _pyparam_typeconvert(PyObject * strvalue, PyTypeObject * type, int check_only) {
	// TODO Kevin: Using this to check the types of object is likely against
	// PEP 20 -- The Zen of Python: "Explicit is better than implicit"

	PyObject * valuetuple = PyTuple_Pack(1, strvalue);
	PyObject * converted_value = PyObject_CallObject((PyObject *)type, valuetuple);
	if (converted_value == NULL) {
		Py_DECREF(valuetuple);  // converted_value is NULL, so we don't decrements its refcount.
		return NULL;  // We assume failed conversions to have set an exception string.
	}
	Py_DECREF(valuetuple);
	if (check_only) {
		Py_DECREF(converted_value);
		Py_RETURN_NONE;
	}
	return converted_value;
}

/* Iterates over the specified iterable, and checks the type of each object. */
static int _pyparam_typecheck_sequence(PyObject * sequence, PyTypeObject * type) {
	// This is likely not thread-safe however.

	// It seems that tuples pass PySequence_Check() but not PyIter_Check(),
	// which seems to indicate that not all sequences are inherently iterable.
	if (!PyIter_Check(sequence) && !PySequence_Check(sequence)) {
		PyErr_SetString(PyExc_TypeError, "Provided value is not a iterable");
		return -1;
	}

	PyObject *iter = PyObject_GetIter(sequence);

	PyObject *item;

	while ((item = PyIter_Next(iter)) != NULL) {

		if (!_pyparam_typeconvert(item, type, 1)) {
#if 0  // Should we raise the exception from the failed conversion, or our own?
			PyObject * temppystr = PyObject_Str(item);
			char* tempstr = (char*)PyUnicode_AsUTF8(temppystr);
			char buf[70 + strlen(item->ob_type->tp_name) + strlen(tempstr)];
			sprintf(buf, "Iterable contains object of an incorrect/unconvertible type <%s: %s>.", item->ob_type->tp_name, tempstr);
			PyErr_SetString(PyExc_TypeError, buf);
			Py_DECREF(temppystr);
#endif
			return -2;
		}
	}

	// Raise an exception if we got an error while iterating.
	return PyErr_Occurred() ? -3 : 0;
}

// TODO Kevin: Some of the changes this function makes, 
//	might not be compatible with how parameters are supposed to be set.
/* Private interface for setting the value of a normal parameter. 
   Use INT_MIN as no offset. */
static int _pyparam_util_set_single(param_t *param, PyObject *value, int offset, param_queue_t *queue) {
	
	if (offset != INT_MIN) {
		if (_pyparam_util_index(param->array_size, &offset))  // Validate the offset.
			return -1;  // Raises IndexError.
	} else
		offset = -1;

	char valuebuf[128] __attribute__((aligned(16))) = { };
	PyObject * strvalue = _pyparam_get_str_value(value);
	param_str_to_value(param->type, (char*)PyUnicode_AsUTF8(strvalue), valuebuf);
	Py_DECREF(strvalue);

	if (queue == NULL) {  // Set the parameter immediately, if no queue is provided.

		if (param->node == PARAM_LIST_LOCAL) {
			if (offset < 0)
				offset = 0;
			param_set(param, offset, valuebuf);

		} else {
			if (param_push_single(param, offset, valuebuf, 1, param->node, 1000, paramver) < 0) {
				PyErr_SetString(PyExc_ConnectionError, "No response");
				return -2;
			}
		}

		param_print(param, offset, NULL, 0, 2);

	} else {  // Otherwise; use the queue.

		if (!queue->buffer) {
			PyErr_SetString(PyExc_SystemError, "Attempted to add parameter to uninitialized queue");
			return -3;
		}
		if (param_queue_add(queue, param, offset, valuebuf) < 0)
			printf("Queue full\n");

	}

	return 0;
}

/* Private interface for setting the value of an array parameter. */
static int _pyparam_util_set_array(param_t *param, PyObject *value) {

	// Transform lazy generators and iterators into sequences,
	// such that their length may be retrieved in a uniform manner.
	// This comes at the expense of memory (and likely performance),
	// especially for very large sequences.
	if (!PySequence_Check(value)) {
		if (PyIter_Check(value)) {
			PyObject * temptuple = PyTuple_Pack(1, value);
			value = PyObject_CallObject((PyObject *)&PyTuple_Type, temptuple);
			Py_DECREF(temptuple);
		} else {
			PyErr_SetString(PyExc_TypeError, "Provided argument must be iterable.");
			return -1;
		}
	} else
		Py_INCREF(value);  // Iterators will be 1 higher than needed so do the same for sequences.

	int seqlen = PySequence_Fast_GET_SIZE(value);

	// We don't support assigning slices (or anything of the like) yet, so...
	if (seqlen != param->array_size) {
		if (param->array_size > 0) {  // Check that the lengths match.
			char buf[120];
			sprintf(buf, "Provided iterable's length does not match parameter's. <iterable length: %i> <param length: %i>", seqlen, param->array_size);
			PyErr_SetString(PyExc_ValueError, buf);
		} else  // Check that the parameter is an array.
			PyErr_SetString(PyExc_TypeError, "Cannot assign iterable to non-array type parameter.");
		Py_DECREF(value);
		return -2;
	}

	// Check that the iterable only contains valid types.
	if (_pyparam_typecheck_sequence(value, _pyparam_misc_param_t_type(param))) {
		Py_DECREF(value);
		return -3;  // Raises TypeError.
	}

	// TODO Kevin: This does not allow for queued operations on array parameters.
	//	This could be implemented by simply replacing 'param_queue_t queue = { };',
	//	with the global queue, but then we need to handle freeing the buffer.
	// TODO Kevin: Also this queue is not used for local parameters (and therefore wasted).
	//	Perhaps structure the function to avoid its unecessary instantiation.
	void * queuebuffer = malloc(PARAM_SERVER_MTU);
	param_queue_t queue = { };
	param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, paramver);
	
	for (int i = 0; i < seqlen; i++) {

		PyObject *item = PySequence_Fast_GET_ITEM(value, i);

		if(!item) {
			Py_DECREF(value);
			free(queuebuffer);
			PyErr_SetString(PyExc_RuntimeError, "Iterator went outside the bounds of the iterable.");
			Py_DECREF(value);
			return -4;
		}

		// Set local parameters immediately, use the global queue if autosend if off.
		param_queue_t *usequeue = (!autosend ? &param_queue_set : ((param->node != PARAM_LIST_LOCAL) ? &queue : NULL));
		_pyparam_util_set_single(param, item, i, usequeue);
		
		// 'item' is a borrowed reference, so we don't need to decrement it.
	}

	param_queue_print((autosend ? &queue : &param_queue_set));
	
	if (param->node != PARAM_LIST_LOCAL)
		if (param_push_queue(&queue, 1, param->node, 100) < 0) {
			PyErr_SetString(PyExc_ConnectionError, "No response.");
			free(queuebuffer);
			Py_DECREF(value);
			return -6;
		}
	
	free(queuebuffer);
	Py_DECREF(value);
	return 0;
}


static PyObject * pyparam_param_set(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	PyObject * value;
	int node = default_node;
	int offset = INT_MIN;  // Using INT_MIN as the least likely value as Parameter arrays should be backwards subscriptable like lists.


	static char *kwlist[] = {"param_identifier", "value", "node", "offset", NULL};
	
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "OO|iii", kwlist, &param_identifier, &value, &node, &offset)) {
		return NULL;  // TypeError is thrown
	}

	param_t *param = _pyparam_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

	if((PyIter_Check(value) || PySequence_Check(value)) && !PyObject_TypeCheck(value, &PyUnicode_Type)) {
		if (_pyparam_util_set_array(param, value))
			return NULL;  // Raises one of many possible exceptions.
	} else {
		param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
		if (_pyparam_util_set_single(param, value, offset, usequeue))
			return NULL;  // Raises one of many possible exceptions.
		param_print(param, -1, NULL, 0, 2);
	}

	Py_RETURN_NONE;
}

static PyObject * pyparam_param_pull(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
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
		return NULL;
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
		return NULL;
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
		return NULL;
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
		return NULL;
	}

	printf("%s\n%s\n%s\n%s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);

	return PyUnicode_FromFormat("%s\n%s\n%s\n%s %s\n", message.ident.hostname, message.ident.model, message.ident.revision, message.ident.date, message.ident.time);
	
}

static PyObject * pyparam_csp_reboot(PyObject * self, PyObject * args) {

	unsigned int node;

	if (!PyArg_ParseTuple(args, "I", &node))
		return NULL;  // Raises TypeError.

	csp_reboot(node);

	Py_RETURN_NONE;
}


static PyObject * pyparam_vmem_list(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
	}

	int node = csp_get_address();
	int timeout = 2000;

	static char *kwlist[] = {"node", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|ii", kwlist, &node, &timeout))
		return NULL;  // Raises TypeError.

	printf("Requesting vmem list from node %u timeout %u\n", node, timeout);


	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		return NULL;
	}

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Failed to get CSP buffer");
		return NULL;
	}

	vmem_request_t * request = (void *) packet->data;
	request->version = VMEM_VERSION;
	request->type = VMEM_SERVER_LIST;
	packet->length = sizeof(vmem_request_t);

	csp_send(conn, packet);

	/* Wait for response */
	packet = csp_read(conn, timeout);
	if (packet == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response.");
		csp_close(conn);
		return NULL;
	}

	PyObject * list_string = PyUnicode_New(0, 0);

	for (vmem_list_t * vmem = (void *) packet->data; (intptr_t) vmem < (intptr_t) packet->data + packet->length; vmem++) {
		char buf[300];
		sprintf(buf, " %u: %-5.5s 0x%08X - %u typ %u\r\n", vmem->vmem_id, vmem->name, (unsigned int) be32toh(vmem->vaddr), (unsigned int) be32toh(vmem->size), vmem->type);
		printf("%s", buf);
		PyUnicode_AppendAndDel(&list_string, PyUnicode_FromString(buf));
	}

	csp_buffer_free(packet);
	csp_close(conn);


	return list_string;
}

static PyObject * pyparam_vmem_restore(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
	}

	// The node argument is required (even though it has a default)
	// because Python requires that keyword/optional arguments 
	// come after positional/required arguments (vmem_id).
	int node = csp_get_address();
	int vmem_id;
	int timeout = 2000;

	static char *kwlist[] = {"node", "vmem_id", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii|i", kwlist, &node, &vmem_id, &timeout))
		return NULL;  // Raises TypeError.

	printf("Restoring vmem %u on node %u\n", vmem_id, node);

	int result = vmem_client_backup(node, vmem_id, timeout, 0);

	if (result == -2) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	} else {
		printf("Result: %d\n", result);
	}

	return Py_BuildValue("i", result);
}

static PyObject * pyparam_vmem_backup(PyObject * self, PyObject * args, PyObject * kwds) {
	
	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
	}

	// The node argument is required (even though it has a default)
	// because Python requires that keyword/optional arguments 
	// come after positional/required arguments (vmem_id).
	int node = csp_get_address();
	int vmem_id;
	int timeout = 2000;

	static char *kwlist[] = {"node", "vmem_id", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ii|i", kwlist, &node, &vmem_id, &timeout))
		return NULL;  // Raises TypeError.

	printf("Taking backup of vmem %u on node %u\n", vmem_id, node);

	int result = vmem_client_backup(node, vmem_id, timeout, 1);

	if (result == -2) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	} else {
		printf("Result: %d\n", result);
	}
	
	return Py_BuildValue("i", result);
}

static PyObject * pyparam_vmem_unlock(PyObject * self, PyObject * args, PyObject * kwds) {
	// TODO Kevin: This function is likely to be very short lived.
	//	As this way of unlocking the vmem is obsolete.

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return NULL;
	}

	int node = csp_get_address();
	int timeout = 2000;

	static char *kwlist[] = {"node", "timeout", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "i|i", kwlist, &node, &timeout))
		return NULL;  // Raises TypeError.

	/* Step 0: Prepare request */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, VMEM_PORT_SERVER, timeout, CSP_O_NONE);
	if (conn == NULL) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	}

	csp_packet_t * packet = csp_buffer_get(sizeof(vmem_request_t));
	if (packet == NULL) {
		PyErr_SetString(PyExc_MemoryError, "Failed to get CSP buffer");
		return NULL;
	}

	vmem_request_t * request = (void *) packet->data;
	request->version = 1;
	request->type = VMEM_SERVER_UNLOCK;
	packet->length = sizeof(vmem_request_t);

	/* Step 1: Check initial unlock code */
	request->unlock.code = htobe32(0x28140360);

	/* Step 2: Wait for verification sequence */
	if ((packet = csp_read(conn, timeout)) == NULL) {
		csp_close(conn);
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	}

	request = (void *) packet->data;
	uint32_t sat_verification = be32toh(request->unlock.code);

	// We skip and simulate successful user input.
	// We still print the output, 
	// if for no other reason than to keep up appearances.

	printf("Verification code received: %x\n\n", (unsigned int) sat_verification);

	printf("************************************\n");
	printf("* WARNING WARNING WARNING WARNING! *\n");
	printf("* You are about to unlock the FRAM *\n");
	printf("* Please understand the risks      *\n");
	printf("* Abort now by typing CTRL + C     *\n");
	printf("************************************\n");

	/* Step 2a: Ask user to input sequence */
	printf("Type verification sequence (you have <30 seconds): \n");

	/* Step 2b: Ask for final confirmation */
	printf("Validation sequence accepted\n");

	printf("Are you sure [Y/N]?\n");

	/* Step 3: Send verification sequence */
	request->unlock.code = htobe32(sat_verification);

	csp_send(conn, packet);

	/* Step 4: Check for result */
	if ((packet = csp_read(conn, timeout)) == NULL) {
		csp_close(conn);
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return NULL;
	}

	request = (void *) packet->data;
	uint32_t result = be32toh(request->unlock.code);
	printf("Result: %x\n", (unsigned int) result);

	csp_close(conn);
	return Py_BuildValue("i", result);
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

	param_t * param = _pyparam_util_find_param_t(param_identifier, node);

	if (param == NULL)  // Did not find a match.
		return NULL;  // Raises TypeError or ValueError.

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

	self->type = (PyTypeObject *)pyparam_misc_get_type((PyObject *)self, NULL);

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

	param_t * param = _pyparam_util_find_param_t(self->name, node);

	if (param == NULL)  // Did not find a match.
		return -1;  // Raises either TypeError or ValueError.

	self->param = param;

	return 0;
}

static PyObject * Parameter_gettype(ParameterObject *self, void *closure) {
	Py_INCREF(self->type);
	return (PyObject *)self->type;
}

static PyObject * Parameter_getvalue(ParameterObject *self, void *closure) {
	if (self->param->array_size > 0)
		return _pyparam_util_get_array(self->param, autosend);
	return _pyparam_util_get_single(self->param, INT_MIN, autosend);
}

static int Parameter_setvalue(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the value attribute");
        return -1;
    }

	if (self->param->array_size > 0 && self->param->type != PARAM_TYPE_STRING)  // Is array parameter
		return _pyparam_util_set_array(self->param, value);
	param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
	return _pyparam_util_set_single(self->param, value, INT_MIN, usequeue);  // Normal parameter
}

static PyObject * Parameter_is_array(ParameterObject *self, void *closure) {
	// I believe this is the most appropriate way to check whether the parameter is an array.
	// Additionally; this seems a decent substitute for an array parameter subclass.
	PyObject * result = self->param->array_size > 0 ? Py_True : Py_False;
	Py_INCREF(result);
	return result;
}

static PyObject * Parameter_getmask(ParameterObject *self, void *closure) {
	return Py_BuildValue("I", self->param->mask);
}

static PyObject * Parameter_gettimestamp(ParameterObject *self, void *closure) {
	return Py_BuildValue("I", self->param->timestamp);
}

static PyObject * Parameter_str(ParameterObject *self) {
	char buf[100];
	sprintf(buf, "[id:%i|node:%i] %s | %s", self->param->id, self->param->node, self->param->name, self->type->tp_name);
	return Py_BuildValue("s", buf);
}

static PyObject * Parameter_GetItem(ParameterObject *self, PyObject *item) {

	if (!PyLong_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "Index must be an integer.");
        return NULL;
	}

	// _PyLong_AsInt is dependant on the Py_LIMITED_API macro, hence the underscore prefix.
	int index = _PyLong_AsInt(item);

	// We can't use the fact that _PyLong_AsInt() returns -1 for error,
	// because it may be our desired index. So we check for an exception instead.
	if (PyErr_Occurred())
		return NULL;  // 'Reraise' the current exception.

	return _pyparam_util_get_single(self->param, index, autosend);
}

static int Parameter_SetItem(ParameterObject *self, PyObject* item, PyObject* value) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete parameter array indexes.");
        return -1;
    }

	if (!PyLong_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "Index must be an integer.");
        return -2;
	}

	// _PyLong_AsInt is dependant on the Py_LIMITED_API macro, hence the underscore prefix.
	int index = _PyLong_AsInt(item);

	// We can't use the fact that _PyLong_AsInt() returns -1 for error,
	// because it may be our desired index. So we check for an exception instead.
	if (PyErr_Occurred())
		return -3;  // 'Reraise' the current exception.

	// _pyparam_util_set_single() uses negative numbers for exceptions,
	// so we just return its return value.
	param_queue_t *usequeue = autosend ? NULL : &param_queue_set;
	return _pyparam_util_set_single(self->param, value, index, usequeue);
}

static Py_ssize_t Parameter_length(ParameterObject *self) {
	// We currently raise an exception when getting len() of non-array type parameters.
	// This stops PyCharm (Perhaps other IDE's) from showing their length as 0. ¯\_(ツ)_/¯
	if (self->param->array_size <= 0) {
		PyErr_SetString(PyExc_AttributeError, "Non-array type parameter is not subscriptable");
		return -1;
	}
	return self->param->array_size;
}

/* 1 for success. Comapares the wrapped param_t for parameters, otherwise 0. Assumes self to be a ParameterObject. */
static int Parameter_equal(PyObject *self, PyObject *other) {
	if (PyObject_TypeCheck(other, &ParameterType) && ((ParameterObject *)other)->param == ((ParameterObject *)self)->param)
		return 1;
	return 0;
}

static PyObject * Parameter_richcompare(PyObject *self, PyObject *other, int op) {

	PyObject *result = Py_NotImplemented;

	switch (op) {
		// case Py_LT:
		// 	break;
		// case Py_LE:
		// 	break;
		case Py_EQ:
			result = (Parameter_equal(self, other)) ? Py_True : Py_False;
			break;
		case Py_NE:
			result = (Parameter_equal(self, other)) ? Py_False : Py_True;
			break;
		// case Py_GT:
		// 	break;
		// case Py_GE:
		// 	break;
	}

    Py_XINCREF(result);
    return result;
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
    {"name", (getter)Parameter_getname, NULL,
     "name of the parameter", NULL},
    {"unit", (getter)Parameter_getunit, NULL,
     "unit of the parameter", NULL},
	{"id", (getter)Parameter_getid, NULL,
     "id of the parameter", NULL},
	{"node", (getter)Parameter_getnode, (setter)Parameter_setnode,
     "node of the parameter", NULL},
	{"type", (getter)Parameter_gettype, NULL,
     "type of the parameter", NULL},
	{"value", (getter)Parameter_getvalue, (setter)Parameter_setvalue,
     "value of the parameter", NULL},
	{"is_array", (getter)Parameter_is_array, NULL,
     "whether the parameter is an array", NULL},
	{"mask", (getter)Parameter_getmask, NULL,
     "mask of the parameter", NULL},
	{"timestamp", (getter)Parameter_gettimestamp, NULL,
     "timestamp of the parameter", NULL},
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
	.tp_richcompare = (richcmpfunc)Parameter_richcompare,
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
		return NULL;
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
		return NULL;
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

		if (PyObject_TypeCheck(item, &ParameterType)) {  // Sanity check
			if (strlen(((ParameterObject *)item)->valuebuf) != 0)  // Empty value buffers, seem to cause errors.
				param_queue_add(&queue, ((ParameterObject *)item)->param, -1, ((ParameterObject *)item)->valuebuf);
		} else
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
		return NULL;
	}

	static char *kwlist[] = {
		"csp_version", "csp_hostname", "csp_model", 
		"use_prometheus", "rtable", "yamlname", "dfl_addr", "quiet", NULL,
	};

	csp_conf.version = 2;
	csp_conf.hostname = "python_bindings";
	csp_conf.model = "linux";

	int use_prometheus = 0;
	char * rtable = NULL;
	char * yamlname = "can.yaml";
	unsigned int dfl_addr = 0;
	
	int quiet = 0;
	

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|BssissIi", kwlist,
		&csp_conf.version,  &csp_conf.hostname, 
		&csp_conf.model, &use_prometheus, &rtable,
		&yamlname, &dfl_addr, &quiet)
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

	csp_yaml_init(yamlname, &dfl_addr);
	// param_set_local_node(dfl_addr);

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);

	if (rtable && csp_rtable_check(rtable)) {
		int error = csp_rtable_load(rtable);
		if (error < 1) {
			printf("csp_rtable_load(%s) failed, error: %d\n", rtable, error);
		}
	}

	csp_bind_callback(csp_service_handler, CSP_ANY);
	csp_bind_callback(param_serve, PARAM_PORT_SERVER);

	/* Start a collector task */
	vmem_file_init(&vmem_col);

	static pthread_t param_collector_handle;
	pthread_create(&param_collector_handle, NULL, &param_collector_task, NULL);

	static pthread_t router_handle;
	pthread_create(&router_handle, NULL, &router_task, NULL);

	static pthread_t vmem_server_handle;
	pthread_create(&vmem_server_handle, NULL, &vmem_server_task, NULL);

	// if (use_prometheus) {  // TODO Kevin: include functions.
	// 	prometheus_init();
	// 	param_sniffer_init();
	// }
	
	_csp_initialized = 1;
	Py_RETURN_NONE;

}

static PyMethodDef methods[] = {

	/* Converted CSH commands from param/param_slash.c */
	{"set", 		(PyCFunction)pyparam_param_set, METH_VARARGS | METH_KEYWORDS, 	""},
	{"get", 		(PyCFunction)pyparam_param_get, METH_VARARGS | METH_KEYWORDS, 	""},
	{"push", 		pyparam_param_push, 			METH_VARARGS, 					""},
	{"pull", 		(PyCFunction)pyparam_param_pull,METH_VARARGS | METH_KEYWORDS, 	""},
	{"clear", 		pyparam_param_clear, 			METH_NOARGS, 					""},
	{"node", 		pyparam_param_node, 			METH_VARARGS, 					""},
	{"paramver", 	pyparam_param_paramver, 		METH_VARARGS, 					""},
	{"autosend", 	pyparam_param_autosend, 		METH_VARARGS, 					""},
	{"queue", 		pyparam_param_queue, 			METH_NOARGS, 					""},

	/* Converted CSH commands from param/param_list_slash.c */
	{"list", 		pyparam_param_list, 			METH_VARARGS, 					""},
	{"list_download", (PyCFunction)pyparam_param_list_download, METH_VARARGS | METH_KEYWORDS, ""},

	/* Converted CSH commands from slash_csp.c */
	/* Including these here is not entirely optimal, they may be removed. */
	{"ping", 		(PyCFunction)pyparam_csp_ping, 	METH_VARARGS | METH_KEYWORDS, 	""},
	{"ident", 		(PyCFunction)pyparam_csp_ident, METH_VARARGS | METH_KEYWORDS, 	""},
	{"reboot", 		pyparam_csp_reboot, 			METH_VARARGS, 					""},

	/* Miscellaneous utility functions */
	{"get_type", 	pyparam_misc_get_type, 			METH_VARARGS, 					""},

	/* Converted vmem commands. */
	{"vmem_list", 	(PyCFunction)pyparam_vmem_list,   METH_VARARGS | METH_KEYWORDS,	""},
	{"vmem_restore",(PyCFunction)pyparam_vmem_restore,METH_VARARGS | METH_KEYWORDS, ""},
	{"vmem_backup", (PyCFunction)pyparam_vmem_backup, METH_VARARGS | METH_KEYWORDS, ""},
	{"vmem_unlock", (PyCFunction)pyparam_vmem_unlock, METH_VARARGS | METH_KEYWORDS, ""},

	/* Misc */
	{"_param_init", (PyCFunction)pyparam_init, 		METH_VARARGS | METH_KEYWORDS, 	""},

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
