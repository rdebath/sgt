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
#include <math.h>

/*
 * Directly include the functions we're providing. Icky, but saves
 * on makefiles.
 */
#include "lambertw.c"
#include "normal.c"

static PyObject *python_wrapper(PyObject *self, PyObject *args,
				double (*func)(double))
{
    double d;

    if (!PyArg_ParseTuple(args, "d", &d))
	return NULL;
    errno = 0;
    d = func(d);
    if (errno == EDOM) {
	PyErr_SetString(PyExc_ValueError, "math domain error");
	return NULL;
    }
    return Py_BuildValue("d", d);
}

#define WRAP(fn) \
    static PyObject *python_ ## fn(PyObject *self, PyObject *args) \
	    { return python_wrapper(self, args, fn); }

WRAP(W)
WRAP(Wn)
WRAP(erf)			       /* standard Python maths module */
WRAP(erfc)			       /* doesn't provide these, so let's */
WRAP(inverf)
WRAP(inverfc)
WRAP(Phi)
WRAP(Phi0)
WRAP(invPhi)
WRAP(invPhi0)

static PyMethodDef pymaths_methods[] = {
    {"W", python_W, METH_VARARGS,
    "The principal branch of the Lambert W function on the real\n"
    "interval (-1/e,infinity)."},
    {"Wn", python_Wn, METH_VARARGS,
    "The negative branch of the Lambert W function on the real\n"
    "interval (-1/e,0)."},
    {"erf", python_erf, METH_VARARGS,
    "The error function."},
    {"erfc", python_erfc, METH_VARARGS,
    "The complementary error function."},
    {"inverf", python_inverf, METH_VARARGS,
    "The inverse of the error function."},
    {"inverfc", python_inverfc, METH_VARARGS,
    "The inverse of the complementary error function."},
    {"Phi", python_Phi, METH_VARARGS,
    "The cumulative probability function of the standard normal\n"
    "distribution."},
    {"Phi0", python_Phi0, METH_VARARGS,
    "The cumulative probability function of the standard normal\n"
    "distribution, integrated from zero rather than infinity."},
    {"invPhi", python_invPhi, METH_VARARGS,
    "The inverse of the cumulative probability function of the\n"
    " standard normal distribution."},
    {"invPhi0", python_invPhi0, METH_VARARGS,
    "The inverse of the cumulative probability function of the\n"
    "standard normal distribution integrated from zero."},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initpymaths(void)
{
    (void)Py_InitModule("pymaths", pymaths_methods);
}
