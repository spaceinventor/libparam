/*
 * parameter.c
 *
 * Contains miscellaneous utilities used by SiPyParam.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#include "utils.h"

#include <dirent.h>
#include <param/param_server.h>
#include <param/param_client.h>
#include <param/param_string.h>

#include "sipyparam.h"
#include "parameter/parameter.h"
#include "parameter/parameterarray.h"
#include "parameter/pythonparameter.h"
#include "parameter/parameterlist.h"
#include "parameter/pythonarrayparameter.h"

#undef NDEBUG
#include <assert.h>

void cleanup_free(void *const* obj) {
    if (*obj == NULL) {
        return;
	}
    free(*obj);
	/* Setting *obj=NULL should never have a visible effect when using __attribute__((cleanup()).
		But accepting a *const* allows for more use cases. */
    //*obj = NULL;  // 
}
/* __attribute__(()) doesn't like to treat char** and void** interchangeably. */
void cleanup_str(char *const* obj) {
    cleanup_free((void *const*)obj);
}
void _close_dir(DIR *const* dir) {
	if (dir == NULL || *dir == NULL) {
		return;
	}
	closedir(*dir);
	//*dir = NULL;
}
void cleanup_GIL(PyGILState_STATE * gstate) {
	//printf("AAA %d\n", PyGILState_Check());
    //if (*gstate == PyGILState_UNLOCKED)
    //    return
    PyGILState_Release(*gstate);
    //*gstate = NULL;
}
void cleanup_pyobject(PyObject **obj) {
    Py_XDECREF(*obj);
}

void state_release_GIL(PyThreadState ** state) {
	if (*state == NULL) {
		return;  // We didn't have the GIL, so there's nothing to release.
	}
    *state = PyEval_SaveThread();
}

__attribute__((malloc(free, 1)))
char *safe_strdup(const char *s) {
    if (s == NULL) {
        return NULL;
    }
    return strdup(s);
}

/* Source: https://pythonextensionpatterns.readthedocs.io/en/latest/super_call.html */
PyObject * call_super_pyname_lookup(PyObject *self, PyObject *func_name, PyObject *args, PyObject *kwargs) {
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

/**
 * @brief Get the first base-class with a .tp_dealloc() different from the specified class.
 * 
 * If the specified class defines its own .tp_dealloc(),
 * if should be safe to assume the returned class to be no more abstract than object(),
 * which features its .tp_dealloc() that ust be called anyway.
 * 
 * This function is intended to be called in a subclassed __del__ (.tp_dealloc()),
 * where it will mimic a call to super().
 * 
 * @param cls Class to find a super() .tp_dealloc() for.
 * @return PyTypeObject* super() class.
 */
PyTypeObject * sipyparam_get_base_dealloc_class(PyTypeObject *cls) {
	
	/* Keep iterating baseclasses until we find one that doesn't use this deallocator. */
	PyTypeObject *baseclass = cls->tp_base;
	for (; baseclass->tp_dealloc == cls->tp_dealloc; (baseclass = baseclass->tp_base));

    assert(baseclass->tp_dealloc != NULL);  // Assert that Python installs some deallocator to classes that don't specifically implement one (Whether sipyparam.Parameter or object()).
	return baseclass;
}

/**
 * @brief Goes well with (__DATE__, __TIME__) and (csp_cmp_message.ident.date, csp_cmp_message.ident.time)
 * 
 * 'date' and 'time' are separate arguments, because it's most convenient when working with csp_cmp_message.
 * 
 * @param date __DATE__ or csp_cmp_message.ident.date
 * @param time __TIME__ or csp_cmp_message.ident.time
 * @return New reference to a PyObject* datetime.datetime() from the specified time and dated
 */
PyObject *sipyparam_ident_time_to_datetime(const char * const date, const char * const time) {

	PyObject *datetime_module AUTO_DECREF = PyImport_ImportModule("datetime");
	if (!datetime_module) {
		return NULL;
	}

	PyObject *datetime_class AUTO_DECREF = PyObject_GetAttrString(datetime_module, "datetime");
	if (!datetime_class) {
		return NULL;
	}

	PyObject *datetime_strptime AUTO_DECREF = PyObject_GetAttrString(datetime_class, "strptime");
	if (!datetime_strptime) {
		return NULL;
	}

	//PyObject *datetime_str AUTO_DECREF = PyUnicode_FromFormat("%U %U", self->date, self->time);
	PyObject *datetime_str AUTO_DECREF = PyUnicode_FromFormat("%s %s", date, time);
	if (!datetime_str) {
		return NULL;
	}

	PyObject *format_str AUTO_DECREF = PyUnicode_FromString("%b %d %Y %H:%M:%S");
	if (!format_str) {
		return NULL;
	}

	PyObject *datetime_args AUTO_DECREF = PyTuple_Pack(2, datetime_str, format_str);
	if (!datetime_args) {
		return NULL;
	}

	/* No DECREF, we just pass the new reference (returned by PyObject_CallObject()) to the caller.
		No NULL check either, because the caller has to do that anyway.
		And the only cleanup we need (for exceptions) is already done by AUTO_DECREF */
	PyObject *datetime_obj = PyObject_CallObject(datetime_strptime, datetime_args);

	return datetime_obj;
}

int sipyparam_get_num_accepted_pos_args(const PyObject *function, bool raise_exc) {

	// Suppress the incompatible pointer type warning when AUTO_DECREF is used on subclasses of PyObject*
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
    PyCodeObject *func_code AUTO_DECREF = (PyCodeObject*)PyObject_GetAttrString((PyObject*)function, "__code__");
	// Re-enable the warning
    #pragma GCC diagnostic pop

    if (!func_code || !PyCode_Check(func_code)) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided function must be callable");
        return -1;
    }

    // Check if the function accepts *args
    int accepts_varargs = (func_code->co_flags & CO_VARARGS) ? 1 : 0;

    // Return INT_MAX if *args is present
    if (accepts_varargs) {
        return INT_MAX;
    }

    // Number of positional arguments excluding *args
    int num_pos_args = func_code->co_argcount;
    return num_pos_args;
}


// Source: https://chatgpt.com
int sipyparam_get_num_required_args(const PyObject *function, bool raise_exc) {

	// Suppress the incompatible pointer type warning when AUTO_DECREF is used on subclasses of PyObject*
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
    PyCodeObject *func_code AUTO_DECREF = (PyCodeObject*)PyObject_GetAttrString((PyObject*)function, "__code__");
	// Re-enable the warning
    #pragma GCC diagnostic pop

    if (!func_code || !PyCode_Check(func_code)) {
        if (raise_exc)
            PyErr_SetString(PyExc_TypeError, "Provided callback must be callable");
        return -1;
    }

    int num_required_pos_args = func_code->co_argcount - func_code->co_kwonlyargcount;

    PyObject *defaults AUTO_DECREF = PyObject_GetAttrString((PyObject*)function, "__defaults__");
    Py_ssize_t num_defaults = (defaults && PyTuple_Check(defaults)) ? PyTuple_Size(defaults) : 0;

    int num_non_default_pos_args = num_required_pos_args - (int)num_defaults;
    return num_non_default_pos_args;
}


/* Retrieves a param_t from either its name, id or wrapper object.
   May raise TypeError or ValueError, returned value will be NULL in either case. */
param_t * _sipyparam_util_find_param_t(PyObject * param_identifier, int node) {

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
static PyTypeObject * _sipyparam_misc_param_t_type(param_t * param) {

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
			PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type.");
			break;
	}

	return param_type;  // or NULL (for NotImplementedError).
}


/* Public interface for '_sipyparam_misc_param_t_type()'
   Does not increment the reference count of the found type before returning. */
PyObject * sipyparam_util_get_type(PyObject * self, PyObject * args) {

	PyObject * param_identifier;
	int node = sipyparam_dfl_node;

	param_t * param;

	/* Function may be called either as method on 'Parameter' object or standalone function. */
	
	if (self && PyObject_TypeCheck(self, &ParameterType)) {
		ParameterObject *_self = (ParameterObject *)self;

		node = *_self->param->node;
		param = _self->param;

	} else {
		if (!PyArg_ParseTuple(args, "O|i", &param_identifier, &node)) {
			return NULL;  // TypeError is thrown
		}

		param = _sipyparam_util_find_param_t(param_identifier, node);
	}

	if (param == NULL) {  // Did not find a match.
		return NULL;  // Raises either TypeError or ValueError.
	}


	return (PyObject *)_sipyparam_misc_param_t_type(param);
}

ParameterObject * Parameter_wraps_param(param_t *param) {
	/* TODO Kevin: If it ever becomes possible to assert() the held state of the GIL,
		we would definitely want to do it here. We don't want to use PyGILState_Ensure()
		because the GIL should still be held after returning. */
    assert(param != NULL);

	PyObject *key AUTO_DECREF = PyLong_FromVoidPtr(param);
    ParameterObject *python_param = (ParameterObject*)PyDict_GetItem((PyObject*)param_callback_dict, key);

	return python_param;
}

static PyTypeObject * get_arrayparameter_subclass(PyTypeObject *type) {

	// Get the __subclasses__ method
    PyObject *subclasses_method AUTO_DECREF = PyObject_GetAttrString((PyObject *)type, "__subclasses__");
    if (subclasses_method == NULL) {
        return NULL;
    }

	// NOTE: .__subclasses__() is not recursive, but this is currently not an issue with ParameterArray and PythonArrayParameter

    // Call the __subclasses__ method
    PyObject *subclasses_list AUTO_DECREF = PyObject_CallObject(subclasses_method, NULL);
    if (subclasses_list == NULL) {
        return NULL;
    }

    // Ensure the result is a list
    if (!PyList_Check(subclasses_list)) {
        PyErr_SetString(PyExc_TypeError, "__subclasses__ did not return a list");
        return NULL;
    }

    // Iterate over the list of subclasses
    Py_ssize_t num_subclasses = PyList_Size(subclasses_list);
    for (Py_ssize_t i = 0; i < num_subclasses; i++) {
        PyObject *subclass = PyList_GetItem(subclasses_list, i);  // Borrowed reference
        if (subclass == NULL) {
            return NULL;
        }

		int is_subclass = PyObject_IsSubclass(subclass, (PyObject*)&ParameterArrayType);
        if (is_subclass < 0) {
			return NULL;
		}
		
		PyErr_Clear();
		if (is_subclass) {
			return (PyTypeObject*)subclass;
		}
    }

	PyErr_Format(PyExc_TypeError, "Failed to find ArrayParameter variant of class %s", type->tp_name);
	return NULL;
}

/* Create a Python Parameter object from a param_t pointer directly. */
PyObject * _sipyparam_Parameter_from_param(PyTypeObject *type, param_t * param, const PyObject * callback, int host, int timeout, int retries, int paramver) {
	if (param == NULL) {
 		return NULL;
	}
	// This parameter is already wrapped by a ParameterObject, which we may return instead.
	ParameterObject * existing_parameter;
	if ((existing_parameter = Parameter_wraps_param(param)) != NULL) {
		/* TODO Kevin: How should we handle when: host, timeout, retries and paramver are different for the existing parameter? */
		return (PyObject*)Py_NewRef(existing_parameter);
	}

	if (param->array_size <= 1 && type == &ParameterArrayType) {
		PyErr_SetString(PyExc_TypeError, 
			"Attempted to create a ParameterArray instance, for a non array parameter.");
		return NULL;
	} else if (param->array_size > 1) {  		   // If the parameter is an array.
		type = get_arrayparameter_subclass(type);  // We create a ParameterArray instance instead.
		if (type == NULL) {
			return NULL;
		}
		// If you listen really carefully here, you can hear OOP idealists, screaming in agony.
		// On a more serious note, I'm amazed that this even works at all.
	}

	ParameterObject *self = (ParameterObject *)type->tp_alloc(type, 0);

	if (self == NULL)
		return NULL;

	{   /* Add ourselves to the callback/lookup dictionary */
		PyObject *key AUTO_DECREF = PyLong_FromVoidPtr(param);
		assert(key != NULL);
		assert(!PyErr_Occurred());
		assert(PyDict_GetItem((PyObject*)param_callback_dict, key) == NULL);
		int set_res = PyDict_SetItem((PyObject*)param_callback_dict, key, (PyObject*)self);
		assert(set_res == 0);  // Allows the param_t callback to find the corresponding ParameterObject.
		assert(PyDict_GetItem((PyObject*)param_callback_dict, key) != NULL);
		assert(!PyErr_Occurred());

		assert(self);
		assert(self->ob_base.ob_type);
		/* The parameter linked list should maintain an eternal reference to Parameter() instances, and subclasses thereof (with the exception of PythonParameter() and its subclasses).
			This check should ensure that: Parameter("name") is Parameter("name") == True.
			This check doesn't apply to PythonParameter()'s, because its reference is maintained by .keep_alive */
		int is_pythonparameter = PyObject_IsSubclass((PyObject*)(type), (PyObject*)&PythonParameterType);
        if (is_pythonparameter < 0) {
			assert(false);
			return NULL;
		}

		if (is_pythonparameter) {
			Py_DECREF(self);  // param_callback_dict should hold a weak reference to self
		}
	}

	self->host = host;
	self->param = param;
	self->timeout = timeout;
	self->retries = retries;
	self->paramver = paramver;

	self->type = (PyTypeObject *)sipyparam_util_get_type((PyObject *)self, NULL);

    return (PyObject *) self;
}


/**
 * @brief Return a list of Parameter wrappers similar to the "list" slash command
 * 
 * @param node <0 for all nodes, otherwise only include parameters for the specified node.
 * @return PyObject* Py_NewRef(list[Parameter])
 */
PyObject * sipyparam_util_parameter_list(uint32_t mask, int node, const char * globstr) {

	PyObject * list = PyObject_CallObject((PyObject *)&ParameterListType, NULL);

	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if ((node >= 0) && (*param->node != node)) {
			continue;
		}
		if ((param->mask & mask) == 0) {
			continue;
		}
		int strmatch(const char *str, const char *pattern, int n, int m);  // TODO Kevin: Maybe strmatch() should be in the libparam public API?
		if ((globstr != NULL) && strmatch(param->name, globstr, strlen(param->name), strlen(globstr)) == 0) {
			continue;
		}

		/* CSH does not specify a paramver when listing parameters,
			so we just use 2 as the default version for the created instances. */
		PyObject * parameter AUTO_DECREF = _sipyparam_Parameter_from_param(&ParameterType, param, NULL, INT_MIN, sipyparam_dfl_timeout, 1, 2);
		if (parameter == NULL) {
			Py_DECREF(list);
			return NULL;
		}
		PyObject * argtuple AUTO_DECREF = PyTuple_Pack(1, parameter);
		Py_XDECREF(ParameterList_append(list, argtuple));  // TODO Kevin: DECREF on None doesn't seem right here...
	}

	return list;

}

/* Checks that the specified index is within bounds of the sequence index, raises IndexError if not.
   Supports Python backwards subscriptions, mutates the index to a positive value in such cases. */
static int _sipyparam_util_index(int seqlen, int *index) {
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
PyObject * _sipyparam_util_get_single(param_t *param, int offset, int autopull, int host, int timeout, int retries, int paramver, int verbose) {

	if (offset != INT_MIN) {
		if (_sipyparam_util_index(param->array_size, &offset))  // Validate the offset.
			return NULL;  // Raises IndexError.
	} else
		offset = -1;

	if (autopull && (*param->node != 0)) {

		for (size_t i = 0; i < (retries > 0 ? retries : 1); i++) {
			int param_pull_res;
			Py_BEGIN_ALLOW_THREADS;
			param_pull_res = param_pull_single(param, offset,  CSP_PRIO_NORM, 1, (host != INT_MIN ? host : *param->node), timeout, paramver);
			Py_END_ALLOW_THREADS;
			if (param_pull_res) {
				if (i >= retries-1) {
					PyErr_SetString(PyExc_ConnectionError, "No response");
					return NULL;
				}
			}
		}	
	}

	if (verbose > -1) {
		param_print(param, -1, NULL, 0, 0, 0);
	}

	switch (param->type) {
		case PARAM_TYPE_UINT8:
		case PARAM_TYPE_XINT8: {
			uint8_t val = (offset != -1) ? param_get_uint8_array(param, offset) : param_get_uint8(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("B", val);
		}
		case PARAM_TYPE_UINT16:
		case PARAM_TYPE_XINT16: {
			uint16_t val = (offset != -1) ? param_get_uint16_array(param, offset) :  param_get_uint16(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("H", val);
		}
		case PARAM_TYPE_UINT32:
		case PARAM_TYPE_XINT32: {
			uint32_t val = (offset != -1) ? param_get_uint32_array(param, offset) :  param_get_uint32(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("I", val);
		}
		case PARAM_TYPE_UINT64:
		case PARAM_TYPE_XINT64: {
			uint64_t val = (offset != -1) ? param_get_uint64_array(param, offset) :  param_get_uint64(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("K", val);
		}
		case PARAM_TYPE_INT8: {
			int8_t val = (offset != -1) ? param_get_int8_array(param, offset) : param_get_int8(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("b", val);	
		}
		case PARAM_TYPE_INT16: {
			int16_t val = (offset != -1) ? param_get_int16_array(param, offset) :  param_get_int16(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("h", val);	
		}
		case PARAM_TYPE_INT32: {
			int32_t val = (offset != -1) ? param_get_int32_array(param, offset) :  param_get_int32(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("i", val);	
		}
		case PARAM_TYPE_INT64: {
			int64_t val = (offset != -1) ? param_get_int64_array(param, offset) :  param_get_int64(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("k", val);	
		}
		case PARAM_TYPE_FLOAT: {
			float val = (offset != -1) ? param_get_float_array(param, offset) : param_get_float(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("f", val);	
		}
		case PARAM_TYPE_DOUBLE: {
			double val = (offset != -1) ? param_get_double_array(param, offset) : param_get_double(param);
			if (PyErr_Occurred()) {  // Error may occur during Parameter_getter()
				return NULL;
			}
			return Py_BuildValue("d", val);	
		}
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
			unsigned int size = (param->array_size > 1) ? param->array_size : 1;
			char buf[size];
			param_get_data(param, buf, size);
			return Py_BuildValue("O&", buf);
		}
		default: {
			/* Default case to make the compiler happy. Set error and return */
			break;
		}
	}
	PyErr_SetString(PyExc_NotImplementedError, "Unsupported parameter type for get operation.");
	return NULL;
}

/* Private interface for getting the value of an array parameter
   Increases the reference count of the returned tuple before returning.  */
PyObject * _sipyparam_util_get_array(param_t *param, int autopull, int host, int timeout, int retries, int paramver, int verbose) {

	// Pull the value for every index using a queue (if we're allowed to),
	// instead of pulling them individually.
	if (autopull && *param->node != 0) {
		void * queuebuffer = malloc(PARAM_SERVER_MTU);
		param_queue_t queue = { };
		param_queue_init(&queue, queuebuffer, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_GET, paramver);

		for (int i = 0; i < param->array_size; i++) {
			param_queue_add(&queue, param, i, NULL);
		}

		for (size_t i = 0; i < (retries > 0 ? retries : 1); i++) {
			if (param_pull_queue(&queue, CSP_PRIO_NORM, 0, *param->node, timeout)) {
				PyErr_SetString(PyExc_ConnectionError, "No response.");
				free(queuebuffer);
				return 0;
			}
		}

		free(queuebuffer);
	}
	
	// We will populate this tuple with the values from the indexes.
	PyObject * value_tuple = PyTuple_New(param->array_size);

	for (int i = 0; i < param->array_size; i++) {
		PyObject * item = _sipyparam_util_get_single(param, i, 0, host, timeout, retries, paramver, verbose);

		if (item == NULL) {  // Something went wrong, probably a ConnectionError. Let's abandon ship.
			Py_DECREF(value_tuple);
			return NULL;
		}
		
		PyTuple_SET_ITEM(value_tuple, i, item);
	}
	
	return value_tuple;
}

static PyObject * _sipyparam_get_str_value(PyObject * obj) {

	// This 'if' exists for cases where the value 
	// of a parmeter is assigned from that of another.
	// i.e: 				param1.value = param2
	// Which is equal to:	param1.value = param2.value
	if (PyObject_TypeCheck(obj, &ParameterType)) {
		// Return the value of the Parameter.

		ParameterObject *paramobj = ((ParameterObject *)obj);

		param_t * param = paramobj->param;
		int host = paramobj->host;
		int timeout = paramobj->timeout;
		int retries = paramobj->retries;
		int paramver = paramobj->paramver;

		PyObject * value AUTO_DECREF = param->array_size > 0 ? 
			_sipyparam_util_get_array(param, 0, host, timeout, retries, paramver, -1) :
			_sipyparam_util_get_single(param, INT_MIN, 0, host, timeout, retries, paramver, -1);

		PyObject * strvalue = PyObject_Str(value);
		return strvalue;
	}
	else  // Otherwise use __str__.
		return PyObject_Str(obj);
}

/* Attempts a conversion to the specified type, by calling it. */
static PyObject * _sipyparam_typeconvert(PyObject * strvalue, PyTypeObject * type, int check_only) {
	// TODO Kevin: Using this to check the types of object is likely against
	// PEP 20 -- The Zen of Python: "Explicit is better than implicit"

	PyObject * valuetuple AUTO_DECREF = PyTuple_Pack(1, strvalue);
	PyObject * converted_value = PyObject_CallObject((PyObject *)type, valuetuple);
	if (converted_value == NULL) {
		return NULL;  // We assume failed conversions to have set an exception string.
	}
	if (check_only) {
		Py_DECREF(converted_value);
		Py_RETURN_NONE;
	}
	return converted_value;
}

/* Iterates over the specified iterable, and checks the type of each object. */
static int _sipyparam_typecheck_sequence(PyObject * sequence, PyTypeObject * type) {
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

		if (!_sipyparam_typeconvert(item, type, 1)) {
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

/* Private interface for setting the value of a normal parameter. 
   Use INT_MIN as no offset. */
int _sipyparam_util_set_single(param_t *param, PyObject *value, int offset, int host, int timeout, int retries, int paramver, int remote, int verbose) {
	
	if (offset != INT_MIN) {
		if (param->type == PARAM_TYPE_STRING) {
			PyErr_SetString(PyExc_NotImplementedError, "Cannot set string parameters by index.");
			return -1;
		}

		if (_sipyparam_util_index(param->array_size, &offset))  // Validate the offset.
			return -1;  // Raises IndexError.
	} else
		offset = -1;

	char valuebuf[128] __attribute__((aligned(16))) = { };
 	{   // Stringify the value object
		PyObject * strvalue AUTO_DECREF = _sipyparam_get_str_value(value);
		switch (param->type) {
			case PARAM_TYPE_XINT8:
			case PARAM_TYPE_XINT16:
			case PARAM_TYPE_XINT32:
			case PARAM_TYPE_XINT64:
				// If the destination parameter is expecting a hexadecimal value
				// and the Python object value is of Long type (int), then we need
				// to do a conversion here. Otherwise if the Python value is a string
				// type, then we must expect hexadecimal digits only (including 0x)
				if (Py_IS_TYPE(value, &PyLong_Type)) {
					// Convert the integer value to hexadecimal digits
					char tmp[64];
					snprintf(tmp,64,"0x%lX", PyLong_AsUnsignedLong(value));
					// Convert the hexadecimal C-string into a Python string object
					PyObject *py_long_str = PyUnicode_FromString(tmp);
					// De-reference the original strvalue before assigning a new
					Py_DECREF(strvalue);
					strvalue = py_long_str;
				}
				break;

			default:
				break;
		}
		param_str_to_value(param->type, (char*)PyUnicode_AsUTF8(strvalue), valuebuf);
	}

	int dest = (host != INT_MIN ? host : *param->node);

	// TODO Kevin: The way we set the parameters has been refactored,
	//	confirm that it still behaves like the original (especially for remote host parameters).
	if (remote && (dest != 0)) {  // When allowed, set remote parameter immediately.

		for (size_t i = 0; i < (retries > 0 ? retries : 1); i++) {
			int param_push_res;
			Py_BEGIN_ALLOW_THREADS;  // Only allow threads for remote parameters, as local ones could have Python callbacks.
			// TODO Kevin: Argument for prio?
			param_push_res = param_push_single(param, offset, CSP_PRIO_NORM, valuebuf, 1, dest, timeout, paramver, false);
			Py_END_ALLOW_THREADS;
			if (param_push_res < 0)
				if (i >= retries-1) {
					PyErr_SetString(PyExc_ConnectionError, "No response");
					return -2;
				}
		}

		if (verbose > -1) {
			param_print(param, offset, NULL, 0, 2, 0);
		}

	} else {  // Otherwise; set local cached value.

		if (offset < 0) {
			for (int i = 0; i < param->array_size; i++)
				param_set(param, i, valuebuf);
		} else {
			param_set(param, offset, valuebuf);
		}

		if (PyErr_Occurred()) {
			/* If the exception came from the callback, we should already have chained unto it. */
			// TODO Kevin: We could create a CallbackException class here, to be caught by us and in Python.
			return -3;
		}
	}

	return 0;
}

/* Private interface for setting the value of an array parameter. */
int _sipyparam_util_set_array(param_t *param, PyObject *value, int host, int timeout, int retries, int paramver, int verbose) {

	// Transform lazy generators and iterators into sequences,
	// such that their length may be retrieved in a uniform manner.
	// This comes at the expense of memory (and likely performance),
	// especially for very large sequences.
	if (!PySequence_Check(value)) {
		if (PyIter_Check(value)) {
			PyObject * temptuple AUTO_DECREF = PyTuple_Pack(1, value);
			value = PyObject_CallObject((PyObject *)&PyTuple_Type, temptuple);
		} else {
			PyErr_SetString(PyExc_TypeError, "Provided argument must be iterable.");
			return -1;
		}
	} else
		Py_INCREF(value);  // Iterators will be 1 higher than needed so do the same for sequences.

	int seqlen = PySequence_Fast_GET_SIZE(value);

	// We don't support assigning slices (or anything of the like) yet, so...
	if (seqlen != param->array_size) {
		if (param->array_size > 1) {  // Check that the lengths match.
			char buf[120];
			sprintf(buf, "Provided iterable's length does not match parameter's. <iterable length: %i> <param length: %i>", seqlen, param->array_size);
			PyErr_SetString(PyExc_ValueError, buf);
		} else  // Check that the parameter is an array.
			PyErr_SetString(PyExc_TypeError, "Cannot assign iterable to non-array type parameter.");
		Py_DECREF(value);
		return -2;
	}

	// Check that the iterable only contains valid types.
	if (_sipyparam_typecheck_sequence(value, _sipyparam_misc_param_t_type(param))) {
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

#if 0  /* TODO Kevin: When should we use queues with the new cmd system? */
		// Set local parameters immediately, use the global queue if autosend if off.
		param_queue_t *usequeue = (!autosend ? &param_queue_set : ((*param->node != 0) ? &queue : NULL));
#endif
		_sipyparam_util_set_single(param, item, i, host, timeout, retries, paramver, 1, verbose);
		
		// 'item' is a borrowed reference, so we don't need to decrement it.
	}

	param_queue_print(&queue);
	
	if (*param->node != 0)
		// TODO Kevin: Argument for prio?
		if (param_push_queue(&queue, CSP_PRIO_NORM, 1, *param->node, 100, 0, false) < 0) {  // TODO Kevin: We should probably have a parameter for hwid here.
			PyErr_SetString(PyExc_ConnectionError, "No response.");
			free(queuebuffer);
			Py_DECREF(value);
			return -6;
		}
	
	free(queuebuffer);
	Py_DECREF(value);
	return 0;
}

int sipyparam_parse_param_mask(PyObject * mask_in, uint32_t * mask_out) {

	assert(mask_in != NULL);

	if (PyUnicode_Check(mask_in)) {
		const char * include_mask_str = PyUnicode_AsUTF8(mask_in);
		*mask_out = param_maskstr_to_mask(include_mask_str);
	} else if (PyLong_Check(mask_in)) {
		*mask_out = PyLong_AsUnsignedLong(mask_in);
	} else {
		PyErr_SetString(PyExc_TypeError, "parameter mask must be either str or int");
		return -1;
	}

	return 0;
}
