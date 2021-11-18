// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include <stdio.h>


#include <param/param.h>
#include <vmem/vmem_server.h>
#include <vmem/vmem_ram.h>
#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/arch/csp_thread.h>
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
	// uint16_t id;
	uint16_t node;  // Use this node as the default node for this parameter.
	PyTypeObject *type;  // Best Python representation of the parameter type, i.e 'int' for uint32.
	//uint32_t mask;

	/* Store Python strings for name and unit, to lessen the overhead of converting them from C */
	PyObject *name;
	PyObject *unit;

	param_t *param;
} ParameterObject;


typedef struct {
	PyListObject list;
	// TODO Kevin: Perhaps expand this object, to avoid the weird Python multi inheritance bug, 
	//	when the subclass object is the same size as the superclass.
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


// static PyObject * Error = NULL;

//static int PARAM_POINTER_HAS_BEEN_FREED = 0;  // used to indicate pointer has been freed, because a NULL pointer can't be set.

/* Retrieves a parameter from either its name, id or wrapper object. */
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


static PyObject * pyparam_param_get(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	int host = -1;
	int node = default_node;
	int offset = -1;

	param_t * param;

	/* Function may be called either as method on 'Paramter object' or standalone function. */
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		ParameterObject *_self = (ParameterObject *)self;

		node = _self->param->node;
		offset = _self->param->array_step;  // TODO Kevin: I think the offset corresponds to array step.
		param = _self->param;

	} else {

		static char *kwlist[] = {"param_identifier", "host", "node", "offset", NULL};

		if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|iii", kwlist, &param_identifier, &host, &node, &offset)) {
			return NULL;  // TypeError is thrown
		}

		param = pyparam_util_find_param(param_identifier, node);

	}

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

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
			Py_RETURN_NONE;
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
		Py_RETURN_NONE;
	}

	if (result < 0) {
		PyErr_SetString(PyExc_ConnectionError, "No response");
		return 0;
	}

	switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8:
			return Py_BuildValue("B", param_get_uint8(param));
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16:
			return Py_BuildValue("H", param_get_uint16(param));
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32:
			return Py_BuildValue("I", param_get_uint32(param));
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64:
			return Py_BuildValue("K", param_get_uint64(param));
		case PARAM_TYPE_INT8:
			return Py_BuildValue("b", param_get_uint8(param));
		case PARAM_TYPE_INT16:
			return Py_BuildValue("h", param_get_uint8(param));
		case PARAM_TYPE_INT32:
			return Py_BuildValue("i", param_get_uint8(param));
		case PARAM_TYPE_INT64:
			return Py_BuildValue("k", param_get_uint8(param));
		case PARAM_TYPE_FLOAT:
			return Py_BuildValue("f", param_get_float(param));
		case PARAM_TYPE_DOUBLE:
			return Py_BuildValue("d", param_get_double(param));
		case PARAM_TYPE_STRING: {
			char buf[param->array_size];
			param_get_string(param, &buf, param->array_size);
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

static PyObject * pyparam_param_set(PyObject * self, PyObject * args, PyObject * kwds) {

	if (!_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot perform operations before ._param_init() has been called.");
		return 0;
	}

	PyObject * param_identifier;  // Raw argument object/type passed. Identify its type when needed.
	char * strvalue;
	int host = -1;
	int node = default_node;
	int offset = -1;

	param_t * param;

	/* Function may be called either as method on 'Paramter object' or standalone function. */
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		/* Parse the value from args */
		if (!PyArg_ParseTuple(args, "s", &strvalue)) {
			return NULL;  // TypeError is thrown
		}

		ParameterObject *_self = (ParameterObject *)self;

		//node = _self->param->node;
		node = _self->node;
		// offset = _self->param->array_step;  // TODO Kevin: Haven't decided how best to parse the offset yet.
		param = _self->param;

	} else {

		static char *kwlist[] = {"param_identifier", "strvalue", "host", "node", "offset", NULL};
		
		if (!PyArg_ParseTupleAndKeywords(args, kwds, "Os|iii", kwlist, &param_identifier, &strvalue, &host, &node, &offset)) {
			return NULL;  // TypeError is thrown
		}

		param = pyparam_util_find_param(param_identifier, node);
	}

	if (param == NULL) {  // Did not find a match.
		PyErr_SetString(PyExc_ValueError, "Could not find a matching parameter.");
		return 0;
	}

	char valuebuf[128] __attribute__((aligned(16))) = { };
	param_str_to_value(param->type, strvalue, valuebuf);

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
		return 0;
	}

	param_print(param, -1, NULL, 0, 2);

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

	Py_RETURN_NONE;
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

	Py_RETURN_NONE;

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
	unsigned int size = 1;

	static char *kwlist[] = {"node", "timeout", "size", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|II", kwlist, &node, &timeout, &size)) {
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


static void Parameter_dealloc(ParameterObject *self) {
	Py_XDECREF((PyObject*)&self->type);  // TODO Kevin: In case this works, we need to be really careful when manipulating reference counts of builtin types.
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
	self->node = default_node;
	//int offset = -1;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|H", kwlist, &param_identifier, &self->node)) {
		return NULL;  // TypeError is thrown
	}

	param_t * param = pyparam_util_find_param(param_identifier, self->node);

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
	self->unit = PyUnicode_FromString(param->unit);
	if (self->unit == NULL) {
		Py_DECREF(self);
		return NULL;
	}
	// self->type = &PyLong_Type;
	self->type = (PyTypeObject *)pyparam_misc_param_type((PyObject *)self, NULL);
	Py_INCREF(self->type);  // TODO Kevin: Confirm this is correct.

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
	return Py_BuildValue("H", self->node);
}

static int Parameter_setnode(ParameterObject *self, PyObject *value, void *closure) {

	if (value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Cannot delete the value attribute");
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
	
	self->node = node;

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
	if (!PyUnicode_Check(value)) {
        PyErr_SetString(PyExc_TypeError,
                        "The value attribute must be set as a string");
        return -1;
    }
	PyObject * value_tuple = PyTuple_Pack(1, value);
	pyparam_param_set((PyObject *)self, value_tuple, NULL);
	Py_DECREF(value_tuple);
	return 0;
}

static PyObject * Parameter_str(ParameterObject *self) {
	char buf[100];
	sprintf(buf, "[id:%i|node:%i] %s", self->param->id, self->param->node, self->param->name);
	return Py_BuildValue("s", buf);
}

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
    // .tp_dealloc = (destructor)Parameter_dealloc,
	.tp_getset = Parameter_getsetters,
	.tp_str = (reprfunc)Parameter_str,
};


// static void ParameterList_dealloc(ParameterListObject *self) {
// 	// Get the type of 'self' in case the user has subclassed 'Parameter'.
// 	// Not that this makes a lot of sense to do.
// 	Py_TYPE(self)->tp_free((PyObject *) self);
// }

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
		return 0;
	}

	//Py_DECREF(obj);

	//PyList_Append(self, obj);

	/* 
	Finding the name in the superclass is likely not nearly as efficient 
	as calling list.append() directly. But it is more flexible.
	*/
	PyObject * func_name = Py_BuildValue("s", "append");
	call_super_pyname_lookup(self, func_name, args, NULL);
	Py_DECREF(func_name);
}

static PyMethodDef ParameterList_methods[] = {
    {"append", (PyCFunction) ParameterList_append, METH_VARARGS,
     PyDoc_STR("Add a Parameter to the list.")},
    {NULL},
};

static int ParameterList_init(ParameterListObject *self, PyObject *args, PyObject *kwds)
{
	int seqlen = PySequence_Fast_GET_SIZE(args);

    if (PyList_Type.tp_init((PyObject *) self, NULL, NULL) < 0)
        return -1;
    
	/* We append all the items passed as arguments to self. Checking that they are Parameters as we go. */
	/* Iterate over the objects in args directly, as opposed to copying them first.
		I'm not sure this is entirely legal, considering the GIL and multiprocessing stuff. */
	for (int i = 0; i < seqlen; i++)
	{
		// TODO Kevin: Confirm this doesn't cause a memory leak, as it should be a borrowed reference.
		PyObject *item = PySequence_Fast_GET_ITEM(args, i);

		if(!item) {  // TODO Kevin: Not sure why we do this.
            Py_DECREF(args);
            return -1;
        }

		if (!PyObject_TypeCheck(item, &ParameterType)) {
			char errstr[40 + strlen(Py_TYPE(self)->tp_name)];
			sprintf(errstr, "%ss can only contain Parameters.", Py_TYPE(self)->tp_name);
			PyErr_SetString(PyExc_TypeError, errstr);
			return -1;
		}

		Py_INCREF(item);  // TODO Kevin: Not sure this is necessary, check for memory leaks.
		PyList_Append(self, item);

	}

    return 0;
}

static PyTypeObject ParameterListType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libparam_py3.ParameterList",
	.tp_doc = "Parameter list class with interface to libparam's queue API.",
	.tp_basicsize = sizeof(ParameterListObject), // TODO Kevin: ParameterListObject is not done.
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_init = (initproc)ParameterList_init,
	// .tp_dealloc = (destructor)ParameterList_dealloc,
	.tp_methods = ParameterList_methods,
};


static PyObject * pyparam_csp_init(PyObject * self, PyObject * args, PyObject *kwds) {

	if (_csp_initialized) {
		PyErr_SetString(PyExc_RuntimeError,
			"Cannot initialize multiple instances of libparam bindings. Please use a previous binding.");
			return 0;
	}

	static char *kwlist[] = {"csp_address", "csp_version", "csp_hostname", "csp_model", "csp_port", "can_dev", NULL};

	csp_conf.address = 1;
	csp_conf.version = 2;
	csp_conf.hostname = "python_bindings";
	csp_conf.model = "linux";

	uint16_t csp_port = PARAM_PORT_SERVER;

	char * can_dev = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|HBssHs", kwlist, &csp_conf.address, &csp_conf.version,  &csp_conf.hostname, &csp_conf.model, &csp_port, &can_dev)) {
		return NULL;  // TypeError is thrown
	}

	/* Get csp config from file */
	vmem_file_init(&vmem_csp);

	/* Parameters */
	vmem_file_init(&vmem_params);
	param_list_store_vmem_load(&vmem_params);

	
	csp_init();

	csp_iface_t * default_iface = NULL;
	if (can_dev != NULL) {
		int error = csp_can_socketcan_open_and_add_interface(can_dev, CSP_IF_CAN_DEFAULT_NAME, 1000000, true, &default_iface);
		if (error != CSP_ERR_NONE) {
			csp_log_error("failed to add CAN interface [%s], error: %d", can_dev, error);
		}
	}


	pthread_create(&router_handle, NULL, &router_task, NULL);

	csp_rdp_set_opt(3, 10000, 5000, 1, 2000, 2);

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

	csp_thread_handle_t vmem_handle;
	csp_thread_create(vmem_server_task, "vmem", 2000, NULL, 1, &vmem_handle);

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
	{"_param_init", (PyCFunction)pyparam_csp_init, 	METH_VARARGS | METH_KEYWORDS, 	""},

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
