/*
 * sipyparam.c
 *
 * Setup and initialization for the SiPyParam module.
 *
 *  Created on: Apr 28, 2022
 *      Author: Kevin Wallentin Carlsen
 */

// It is recommended to always define PY_SSIZE_T_CLEAN before including Python.h
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#include "sipyparam.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/utsname.h>

#include <param/param.h>
#ifdef PARAM_HAVE_COMMANDS
#include <param/param_commands.h>
#endif
#ifdef PARAM_HAVE_SCHEDULER
#include <param/param_scheduler.h>
#endif


#include <vmem/vmem_file.h>

#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_hooks.h>
#include <pthread.h>

#include <sys/types.h>

#include "utils.h"

#include "parameter/parameter.h"
#include "parameter/parameterarray.h"
#include "parameter/pythonparameter.h"
#include "parameter/pythonarrayparameter.h"
#include "parameter/pythongetsetparameter.h"
#include "parameter/pythongetsetarrayparameter.h"
#include "parameter/parameterlist.h"


#include "wrapper/param_py.h"
#include "wrapper/spaceboot_py.h"
#include "wrapper/param_list_py.h"
#include "wrapper/vmem_client_py.h"

extern const char *version_string;

VMEM_DEFINE_FILE(col, "col", "colcnf.vmem", 120);
#ifdef PARAM_HAVE_COMMANDS
VMEM_DEFINE_FILE(commands, "cmd", "commands.vmem", 2048);
#endif
#ifdef PARAM_HAVE_SCHEDULER
VMEM_DEFINE_FILE(schedule, "sch", "schedule.vmem", 2048);
#endif
VMEM_DEFINE_FILE(dummy, "dummy", "dummy.txt", 1000000);

// #define PARAMID_CSP_RTABLE					 12
// PARAM_DEFINE_STATIC_VMEM(PARAMID_CSP_RTABLE,      csp_rtable,        PARAM_TYPE_STRING, 64, 0, PM_SYSCONF, NULL, "", csp, 0, NULL);

/* Assertions used when parsing Python arguments, i.e int -> uint32_t */
static_assert(sizeof(unsigned int) == sizeof(uint32_t));

// We include this parameter when testing the behavior of arrays, as none would exist otherwise.
uint8_t _test_array[] = {1,2,3,4,5,6,7,8};
PARAM_DEFINE_STATIC_RAM(1001, test_array_param,          PARAM_TYPE_UINT8,  8, sizeof(uint8_t),  PM_DEBUG, NULL, "", _test_array, "Parameter to use when testing arrays.");

static char _test_str[80];
PARAM_DEFINE_STATIC_RAM(1002, test_str,          PARAM_TYPE_STRING,  80, 1,  PM_DEBUG, NULL, "", _test_str, "Parameter to use when testing strings");


// Keep track of whether init has been run,
// to prevent unexpected behavior from running relevant functions first.
uint8_t _csp_initialized = 0;

uint8_t csp_initialized() {
	return _csp_initialized;
}

unsigned int sipyparam_dfl_verbose = -1;

uint64_t clock_get_nsec(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1E9 + ts.tv_nsec;
}

void * onehz_task(void * param) {
	while(1) {
#ifdef PARAM_HAVE_SCHEDULER
		csp_timestamp_t scheduler_time = {};
        csp_clock_get_time(&scheduler_time);
        param_schedule_server_update(scheduler_time.tv_sec * 1E9 + scheduler_time.tv_nsec);
#endif
		// Py_BEGIN_ALLOW_THREADS;
		sleep(1);
		// Py_END_ALLOW_THREADS;
	}
	return NULL;
}

// TODO Kevin: It's probably not safe to call this function consecutively with the same std_stream or stream_buf.
static int _handle_stream(PyObject * stream_identifier, FILE **std_stream, FILE *stream_buf) {
	if (stream_identifier == NULL)
		return 0;

	if (PyLong_Check(stream_identifier)) {
		long int val = PyLong_AsLong(stream_identifier);
		switch (val) {
			case -2:  // STDOUT
				break;  // Default behavior is correct, don't do anything.
			case -3:  // DEVNULL
				// fclose(stdout);
				if ((stream_buf = fopen("/dev/null", "w")) == NULL) {
					char buf[150];
					snprintf(buf, 150, "Impossible error! Can't open /dev/null: %s\n", strerror(errno));
					PyErr_SetString(PyExc_IOError, buf);
					return -1;
				}
				*std_stream = stream_buf;
				break;
			default:
				PyErr_SetString(PyExc_ValueError, "Argument should be either -2 for subprocess.STDOUT, -3 for subprocess.DEVNULL or a string to a file.");
				return -2;
		}
	} else if (PyUnicode_Check(stream_identifier)) {
		const char *filename = PyUnicode_AsUTF8(stream_identifier);

		if (stream_buf != NULL)
			fclose(stream_buf);

		if ((stream_buf = freopen(filename, "w+", *std_stream)) == NULL) {
			char buf[30 + strlen(filename)];
			sprintf(buf, "Failed to open file: %s", filename);
			PyErr_SetString(PyExc_IOError, buf);
			return -3;
		}
		// std_stream = stream_buf;
	} else {
		PyErr_SetString(PyExc_TypeError, "Argument should be either -2 for subprocess.STDOUT, -3 for subprocess.DEVNULL or a string to a file.");
		return -4;
	}
	return 0;
}

static PyObject * sipyparam_init(PyObject * self, PyObject * args, PyObject *kwds) {

	// if (_csp_initialized) {
	// 	PyErr_SetString(PyExc_RuntimeError,
	// 		"Cannot initialize multiple instances of libparam bindings. Please use a previous binding.");
	// 	return NULL;
	// }

	static char *kwlist[] = {
		"quiet", "stdout", "stderr", NULL,
	};

	int quiet = 0;
	PyObject *csh_stdout = NULL;
	PyObject *csh_stderr = NULL;
	

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iOO:init", kwlist, &quiet, &csh_stdout, &csh_stderr))
		return NULL;  // TypeError is thrown

	// TODO Kevin: Support reassigning streams through module function or global.
	static FILE * temp_stdout_fd = NULL;
	static FILE * temp_stderr_fd = NULL;

	if (quiet) {
		if ((temp_stdout_fd = fopen("/dev/null", "w")) == NULL) {
			fprintf(stderr, "Impossible error! Can't open /dev/null: %s\n", strerror(errno));
			exit(1);
		}
		stdout = temp_stdout_fd;
	}
	else if (
		_handle_stream(csh_stdout, &stdout, temp_stdout_fd) != 0 ||
		_handle_stream(csh_stderr, &stderr, temp_stderr_fd) != 0
	) {
		return NULL;
	}
	#ifndef SIPYPARAM_HAVE_APM

#ifdef PARAM_HAVE_COMMANDS
	vmem_file_init(&vmem_commands);
	param_command_server_init();
#endif
#ifdef PARAM_HAVE_SCHEDULER
	param_schedule_server_init();
#endif

	vmem_file_init(&vmem_dummy);

	static pthread_t onehz_handle;
	pthread_create(&onehz_handle, NULL, &onehz_task, NULL);
	#endif  // SIPYPARAM_HAVE_APM
	
	_csp_initialized = 1;
	Py_RETURN_NONE;

}


static PyMethodDef methods[] = {

	/* Converted CSH commands from libparam/src/param/param_slash.c */
	{"get", 		(PyCFunction)sipyparam_param_get, 	METH_VARARGS | METH_KEYWORDS, "Set the value of a parameter."},
	{"set", 		(PyCFunction)sipyparam_param_set, 	METH_VARARGS | METH_KEYWORDS, "Get the value of a parameter."},
	// {"push", 		(PyCFunction)sipyparam_param_push,	METH_VARARGS | METH_KEYWORDS, "Push the current queue."},
	{"pull", 		(PyCFunction)sipyparam_param_pull,	METH_VARARGS | METH_KEYWORDS, "Pull all or a specific set of parameters."},
	{"cmd_done", 	sipyparam_param_cmd_done, 			METH_NOARGS, 				  "Clears the queue."},
	{"cmd_new", 	(PyCFunction)sipyparam_param_cmd_new,METH_VARARGS | METH_KEYWORDS, "Create a new command"},
#if 0  // TODO Kevin: These bindings should be implemented in a sipyslash, if we want to continue to support global state. 
	{"node", 		sipyparam_slash_node, 			  	METH_VARARGS, 				  "Used to get or change the default node."},
	{"timeout", 	sipyparam_slash_timeout, 			METH_VARARGS, 		  		  "Used to get or change the default timeout."},
	{"verbose", 	sipyparam_slash_verbose, 			METH_VARARGS, 		  		  "Used to get or change the default parameter verbosity."},
#endif
	{"queue", 		sipyparam_param_cmd,			  	METH_NOARGS, 				  "Print the current command."},

	/* Converted CSH commands from libparam/src/param/list/param_list_slash.c */
	{"list", 		(PyCFunction)sipyparam_param_list,	METH_VARARGS | METH_KEYWORDS, "List all known parameters."},
	{"list_download", (PyCFunction)sipyparam_param_list_download, METH_VARARGS | METH_KEYWORDS, "Download all parameters on the specified node."},
	{"list_forget", (PyCFunction)sipyparam_param_list_forget, 	  METH_VARARGS | METH_KEYWORDS, "Remove remote parameters, matching the provided arguments, from the global list."},
	{"list_save", 	(PyCFunction)sipyparam_param_list_save, 	  METH_VARARGS | METH_KEYWORDS, "Save a list of parameters to a file."},
	{"list_add", 	(PyCFunction)sipyparam_param_list_add, 	      METH_VARARGS | METH_KEYWORDS, "Add a paramter to the global list."},

	// {"list_load", 	sipyparam_param_list_load, 		  	METH_VARARGS, 				  "Load a list of parameters from a file."},

	/* Utility functions */
	{"get_type", 	sipyparam_util_get_type, 		  	METH_VARARGS, 				  "Gets the type of the specified parameter."},

	/* Converted vmem commands from libparam/src/vmem/vmem_client_slash.c */
	{"vmem", 	(PyCFunction)sipyparam_param_vmem,   METH_VARARGS | METH_KEYWORDS, "Builds a string of the vmem at the specified node."},

	/* Converted vmem commands from libparam/src/vmem/vmem_client.c */
	{"vmem_download", (PyCFunction)sipyparam_vmem_download,   METH_VARARGS | METH_KEYWORDS, "Download a vmem area."},
	{"vmem_upload", (PyCFunction)sipyparam_vmem_upload,   METH_VARARGS | METH_KEYWORDS, "Upload data to a vmem area."},

	/* Converted program/reboot commands from csh/src/spaceboot_slash.c */
	{"switch", 	(PyCFunction)slash_csp_switch,   METH_VARARGS | METH_KEYWORDS, "Reboot into the specified firmware slot."},
	{"program", (PyCFunction)sipyparam_csh_program,  METH_VARARGS | METH_KEYWORDS, "Upload new firmware to a module."},
	{"sps", 	(PyCFunction)slash_sps,   		 METH_VARARGS | METH_KEYWORDS, "Switch -> Program -> Switch"},

	/* Misc */
	{"init", (PyCFunction)sipyparam_init, 				METH_VARARGS | METH_KEYWORDS, "Initializes the module, with the provided settings."},

	/* sentinel */
	{NULL, NULL, 0, NULL}};

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"sipyparam",
	"Bindings primarily dedicated to the CSH shell interface commands",
	-1,
	methods,
	NULL,
	NULL,
	NULL,
	NULL};

PyMODINIT_FUNC PyInit_sipyparam(void) {

	if (PyType_Ready(&ParameterType) < 0)
        return NULL;

	if (PyType_Ready(&ParameterArrayType) < 0)
        return NULL;

	if (PyType_Ready(&PythonParameterType) < 0)
        return NULL;

	/* PythonArrayParameterType must be created dynamically after
		ParameterArrayType and PythonParameterType to support multiple inheritance. */
	if (create_pythonarrayparameter_type() == NULL)
		return NULL;
	if (PyType_Ready(PythonArrayParameterType) < 0)
		return NULL;

	if (PyType_Ready(&PythonGetSetParameterType) < 0)
        return NULL;

	/* PythonArrayParameterType must be created dynamically after
		ParameterArrayType and PythonParameterType to support multiple inheritance. */
	if (create_pythongetsetarrayparameter_type() == NULL)
		return NULL;
	if (PyType_Ready(PythonGetSetArrayParameterType) < 0)
		return NULL;

	ParameterListType.tp_base = &PyList_Type;
	if (PyType_Ready(&ParameterListType) < 0)
		return NULL;


	PyObject * m = PyModule_Create(&moduledef);
	if (m == NULL)
		return NULL;

	{  /* Exceptions */
		// TODO Kevin: I think Py_IncRef() and PyModule_AddObject() should be replaced with PyModule_AddObjectRef()
		PyExc_ProgramDiffError = PyErr_NewExceptionWithDoc("sipyparam.ProgramDiffError", 
			"Raised when a difference is detected between uploaded/downloaded data after programming.\n"
			"Must be caught before ConnectionError() baseclass.",
			PyExc_ConnectionError, NULL);
		Py_IncRef(PyExc_ProgramDiffError);
		PyModule_AddObject(m, "ProgramDiffError", PyExc_ProgramDiffError);

		PyExc_ParamCallbackError = PyErr_NewExceptionWithDoc("sipyparam.ParamCallbackError", 
			"Raised and chains unto exceptions raised in the callbacks of PythonParameters.\n"
			"Must be caught before RuntimeError() baseclass.",
			PyExc_RuntimeError, NULL);
		Py_IncRef(PyExc_ParamCallbackError);
		PyModule_AddObject(m, "ParamCallbackError", PyExc_ParamCallbackError);

		PyExc_InvalidParameterTypeError = PyErr_NewExceptionWithDoc("sipyparam.InvalidParameterTypeError", 
			"Raised when attempting to create a new PythonParameter() with an invalid type.\n"
			"Must be caught before ValueError() baseclass.",
			PyExc_ValueError, NULL);
		Py_IncRef(PyExc_InvalidParameterTypeError);
		PyModule_AddObject(m, "ParamCallbackError", PyExc_InvalidParameterTypeError);
	}

	Py_INCREF(&ParameterType);
	if (PyModule_AddObject(m, "Parameter", (PyObject *) &ParameterType) < 0) {
		Py_DECREF(&ParameterType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(&ParameterArrayType);
	if (PyModule_AddObject(m, "ParameterArray", (PyObject *) &ParameterArrayType) < 0) {
		Py_DECREF(&ParameterArrayType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(&PythonParameterType);
	if (PyModule_AddObject(m, "PythonParameter", (PyObject *) &PythonParameterType) < 0) {
		Py_DECREF(&PythonParameterType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(PythonArrayParameterType);
    if (PyModule_AddObject(m, "PythonArrayParameter", (PyObject *) PythonArrayParameterType) < 0) {
		Py_DECREF(PythonArrayParameterType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(&PythonGetSetParameterType);
	if (PyModule_AddObject(m, "PythonGetSetParameter", (PyObject *) &PythonGetSetParameterType) < 0) {
		Py_DECREF(&PythonGetSetParameterType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(PythonGetSetArrayParameterType);
    if (PyModule_AddObject(m, "PythonGetSetArrayParameter", (PyObject *) PythonGetSetArrayParameterType) < 0) {
		Py_DECREF(PythonGetSetArrayParameterType);
        Py_DECREF(m);
        return NULL;
	}

	Py_INCREF(&ParameterListType);
	if (PyModule_AddObject(m, "ParameterList", (PyObject *)&ParameterListType) < 0) {
		Py_DECREF(&ParameterListType);
		Py_DECREF(m);
		return NULL;
	}


	{ /* Constants */
		/* Version Control */
		PyModule_AddObject(m, "VERSION", PyUnicode_FromString(version_string));
		PyModule_AddObject(m, "COMPILE_DATE", PyUnicode_FromString(__DATE__));
		PyModule_AddObject(m, "COMPILE_DATETIME", sipyparam_ident_time_to_datetime(__DATE__, __TIME__));

		/* Param Type Enums */
		PyModule_AddObject(m, "PARAM_TYPE_UINT8", PyLong_FromLong(PARAM_TYPE_UINT8));
		PyModule_AddObject(m, "PARAM_TYPE_UINT16", PyLong_FromLong(PARAM_TYPE_UINT16));
		PyModule_AddObject(m, "PARAM_TYPE_UINT32", PyLong_FromLong(PARAM_TYPE_UINT32));
		PyModule_AddObject(m, "PARAM_TYPE_UINT64", PyLong_FromLong(PARAM_TYPE_UINT64));
		PyModule_AddObject(m, "PARAM_TYPE_INT8", PyLong_FromLong(PARAM_TYPE_INT8));
		PyModule_AddObject(m, "PARAM_TYPE_INT16", PyLong_FromLong(PARAM_TYPE_INT16));
		PyModule_AddObject(m, "PARAM_TYPE_INT32", PyLong_FromLong(PARAM_TYPE_INT32));
		PyModule_AddObject(m, "PARAM_TYPE_INT64", PyLong_FromLong(PARAM_TYPE_INT64));
		PyModule_AddObject(m, "PARAM_TYPE_XINT8", PyLong_FromLong(PARAM_TYPE_XINT8));
		PyModule_AddObject(m, "PARAM_TYPE_XINT16", PyLong_FromLong(PARAM_TYPE_XINT16));
		PyModule_AddObject(m, "PARAM_TYPE_XINT32", PyLong_FromLong(PARAM_TYPE_XINT32));
		PyModule_AddObject(m, "PARAM_TYPE_XINT64", PyLong_FromLong(PARAM_TYPE_XINT64));
		PyModule_AddObject(m, "PARAM_TYPE_FLOAT", PyLong_FromLong(PARAM_TYPE_FLOAT));
		PyModule_AddObject(m, "PARAM_TYPE_DOUBLE", PyLong_FromLong(PARAM_TYPE_DOUBLE));
		PyModule_AddObject(m, "PARAM_TYPE_STRING", PyLong_FromLong(PARAM_TYPE_STRING));
		PyModule_AddObject(m, "PARAM_TYPE_DATA", PyLong_FromLong(PARAM_TYPE_DATA));
		PyModule_AddObject(m, "PARAM_TYPE_INVALID", PyLong_FromLong(PARAM_TYPE_INVALID));

		/* Param Mask Enums */
		PyModule_AddObject(m, "PM_READONLY", PyLong_FromLong(PM_READONLY));
		PyModule_AddObject(m, "PM_REMOTE", PyLong_FromLong(PM_REMOTE));
		PyModule_AddObject(m, "PM_CONF", PyLong_FromLong(PM_CONF));
		PyModule_AddObject(m, "PM_TELEM", PyLong_FromLong(PM_TELEM));
		PyModule_AddObject(m, "PM_HWREG", PyLong_FromLong(PM_HWREG));
		PyModule_AddObject(m, "PM_ERRCNT", PyLong_FromLong(PM_ERRCNT));
		PyModule_AddObject(m, "PM_SYSINFO", PyLong_FromLong(PM_SYSINFO));
		PyModule_AddObject(m, "PM_SYSCONF", PyLong_FromLong(PM_SYSCONF));
		PyModule_AddObject(m, "PM_WDT", PyLong_FromLong(PM_WDT));
		PyModule_AddObject(m, "PM_DEBUG", PyLong_FromLong(PM_DEBUG));
		PyModule_AddObject(m, "PM_CALIB", PyLong_FromLong(PM_CALIB));
		PyModule_AddObject(m, "PM_ATOMIC_WRITE", PyLong_FromLong(PM_ATOMIC_WRITE));
		PyModule_AddObject(m, "PM_PRIO1", PyLong_FromLong(PM_PRIO1));
		PyModule_AddObject(m, "PM_PRIO2", PyLong_FromLong(PM_PRIO2));
		PyModule_AddObject(m, "PM_PRIO3", PyLong_FromLong(PM_PRIO3));
		PyModule_AddObject(m, "PM_PRIO_MASK", PyLong_FromLong(PM_PRIO_MASK));

		// TODO Kevin: We should probably add constants for SLASH_SUCCESS and such
	}

	param_callback_dict = (PyDictObject *)PyDict_New();

	return m;
}
