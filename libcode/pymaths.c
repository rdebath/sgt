/*
 * Native-code Python module which provides access from Python to
 * maths library functions I've written in C.
 * 
 * Compile with:
 * 
 * gcc -fpic -c pymaths.c -I/usr/include/python2.4 && ld -shared -o pymaths.so pymaths.o
 * 
 * (or adjust for your local Python variant)
 */

#include <Python.h>

#include "lambertw.c" /* icky, but makes life easier */

static PyObject *python_W(PyObject *self, PyObject *args)
{
    double d;

    if (!PyArg_ParseTuple(args, "d", &d))
	return NULL;
    errno = 0;
    d = W(d);
    if (errno == EDOM) {
	PyErr_SetString(PyExc_ValueError, "math domain error");
	return NULL;
    }
    return Py_BuildValue("d", d);
}

static PyObject *python_Wn(PyObject *self, PyObject *args)
{
    double d;

    if (!PyArg_ParseTuple(args, "d", &d))
	return NULL;
    errno = 0;
    d = Wn(d);
    if (errno == EDOM) {
	PyErr_SetString(PyExc_ValueError, "math domain error");
	return NULL;
    }
    return Py_BuildValue("d", d);
}

static PyMethodDef pymaths_methods[] = {
    {"W", python_W, METH_VARARGS,
    "The principal branch of the Lambert W function on the real\n"
    "interval (-1/e,infinity)."},
    {"Wn", python_Wn, METH_VARARGS,
    "The negative branch of the Lambert W function on the real\n"
    "interval (-1/e,0)."},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initpymaths(void)
{
    (void)Py_InitModule("pymaths", pymaths_methods);
}
