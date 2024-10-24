/*
 * parameter.h
 *
 * Contains miscellaneous utilities used by SiPyParam.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

#pragma once

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <dirent.h>
#include <param/param.h>
#include <param/param_queue.h>
#include "parameter/pythonparameter.h"

void cleanup_free(void *const* obj);
void cleanup_str(char *const* obj);
void _close_dir(DIR *const* dir);
void cleanup_GIL(PyGILState_STATE * gstate);
void cleanup_pyobject(PyObject **obj);

void state_release_GIL(PyThreadState ** state);

#define CLEANUP_FREE __attribute__((cleanup(cleanup_free)))
#define CLEANUP_STR __attribute__((cleanup(cleanup_str)))
#define CLEANUP_DIR __attribute__((cleanup(_close_dir)))
#define CLEANUP_GIL __attribute__((cleanup(cleanup_GIL)))
#define AUTO_DECREF __attribute__((cleanup(cleanup_pyobject)))

__attribute__((malloc(free, 1)))
char *safe_strdup(const char *s);

/* Source: https://pythonextensionpatterns.readthedocs.io/en/latest/super_call.html */
PyObject * call_super_pyname_lookup(PyObject *self, PyObject *func_name, PyObject *args, PyObject *kwargs);

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
PyTypeObject * sipyparam_get_base_dealloc_class(PyTypeObject *cls);

/**
 * @brief Goes well with (__DATE__, __TIME__) and (csp_cmp_message.ident.date, csp_cmp_message.ident.time)
 * 
 * 'date' and 'time' are separate arguments, because it's most convenient when working with csp_cmp_message.
 * 
 * @param date __DATE__ or csp_cmp_message.ident.date
 * @param time __TIME__ or csp_cmp_message.ident.time
 * @return New reference to a PyObject* datetime.datetime() from the specified time and date
 */
PyObject *sipyparam_ident_time_to_datetime(const char * const date, const char * const time);

int sipyparam_get_num_accepted_pos_args(const PyObject *function, bool raise_exc);

int sipyparam_get_num_required_args(const PyObject *function, bool raise_exc);

/* Retrieves a param_t from either its name, id or wrapper object.
   May raise TypeError or ValueError, returned value will be NULL in either case. */
param_t * _sipyparam_util_find_param_t(PyObject * param_identifier, int node);


/* Public interface for '_sipyparam_misc_param_t_type()'
   Increments the reference count of the found type before returning. */
PyObject * sipyparam_util_get_type(PyObject * self, PyObject * args);


/* Create a Python Parameter object from a param_t pointer directly. */
PyObject * _sipyparam_Parameter_from_param(PyTypeObject *type, param_t * param, const PyObject * callback, int host, int timeout, int retries, int paramver);


/**
 * @brief Return a list of Parameter wrappers similar to the "list" slash command
 * 
 * @param node <0 for all nodes, otherwise only include parameters for the specified node.
 * @return PyObject* Py_NewRef(list[Parameter])
 */
PyObject * sipyparam_util_parameter_list(uint32_t mask, int node, const char * globstr);

/* Private interface for getting the value of single parameter
   Increases the reference count of the returned item before returning.
   Use INT_MIN for offset as no offset. */
PyObject * _sipyparam_util_get_single(param_t *param, int offset, int autopull, int host, int timeout, int retries, int paramver, int verbose);

/* Private interface for getting the value of an array parameter
   Increases the reference count of the returned tuple before returning.  */
PyObject * _sipyparam_util_get_array(param_t *param, int autopull, int host, int timeout, int retries, int paramver, int verbose);


/* Private interface for setting the value of a normal parameter. 
   Use INT_MIN as no offset. */
int _sipyparam_util_set_single(param_t *param, PyObject *value, int offset, int host, int timeout, int retries, int paramver, int remote, int verbose);

/* Private interface for setting the value of an array parameter. */
int _sipyparam_util_set_array(param_t *param, PyObject *value, int host, int timeout, int retries, int paramver, int verbose);

/**
 * @brief Check if this param_t is wrapped by a ParameterObject.
 * 
 * @return borrowed reference to the wrapping ParameterObject if wrapped, otherwise NULL.
 */
ParameterObject * Parameter_wraps_param(param_t *param);

/**
 * @brief Convert a python str og int parameter mask to the uint32_t C equivalent.
 * 
 * @param mask_in Python mask to convert
 * @param mask_out Returned parsed parameter mask.
 * @return int 0 on success
 */
int sipyparam_parse_param_mask(PyObject * mask_in, uint32_t * mask_out);
