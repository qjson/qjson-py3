#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdbool.h>
#include "qjson.h"


// Function decode of qjson2json module.
// Given a string containing qjson text, it returns the corresponding json text
// or raise a value error exception if the qjson text is invalid. 
static PyObject *qjson2json_decode(PyObject *self, PyObject *args) {
    const char *inStr;
    if (!PyArg_ParseTuple(args, "s", &inStr))
        return NULL;
	
    // assert inStr is a utf8 encoded C string
	
    // call the wrapped C function returning a heap allocated utf8 c string
    // or NULL when the heap allocation failed.
    const char *outStr = qjson_decode(inStr);
    if (outStr == NULL)
        return PyErr_NoMemory();

    // set isError to true if the returned string is an error
    bool isError = strlen(outStr) > 0 && outStr[0] != '{';

    // convert the returned string into a python unicode string
    PyObject *tmp = PyUnicode_DecodeUTF8(outStr, strlen(outStr), NULL);
    free((char*)outStr);

    // if didn’t return an error, return the string produced by the decoder
    if (!isError) 
        return tmp;

    // raise a value error exception 
    PyErr_SetObject(PyExc_ValueError, tmp);
    return NULL;
}

// Function version of the qjson2json module.
// It returns a string specifying the version of the syntax and the converter. 
static PyObject *qjson2json_version() {
    const char *v = qjson_version();
    return PyUnicode_DecodeUTF8(v, strlen(v), NULL);
 }


// Module’s method table and initialization function. 
static PyMethodDef qjson2json_methods[] = {
    {"decode",  (PyCFunction)qjson2json_decode, METH_VARARGS, 
        "Converts qjson text into json text, or raise a ValueError exception if the qjson text is invalid."},
    {"version", (PyCFunction)qjson2json_version, METH_VARARGS, "Return the syntax and converter version."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

// Module definition structure.
static struct PyModuleDef qjson2jsonmodule = {
    PyModuleDef_HEAD_INIT,
    "qjson2json",   // name of module
    "qjson2json converts qjson text, human readable json, into json text", // module documentation, may be NULL
    -1,        // size of per-interpreter state of the module,
               // or -1 if the module keeps state in global variables.
    qjson2json_methods
};

// Module initialization function.
PyMODINIT_FUNC PyInit_qjson2json(void) {
    return PyModule_Create(&qjson2jsonmodule);
}
