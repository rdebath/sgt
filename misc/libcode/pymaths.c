/*
 * Native-code Python module which provides access from Python to
 * maths library functions I've written in C, and (while I'm at it)
 * to C99-provided maths library functions which Python's own
 * `math' module doesn't bother with.
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

static PyObject *python_wrapper1(PyObject *self, PyObject *args,
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
    } else if (errno == ERANGE) {
	PyErr_SetString(PyExc_OverflowError, "math range error");
	return NULL;
    }
    return Py_BuildValue("d", d);
}

static PyObject *python_wrapper2(PyObject *self, PyObject *args,
				double (*func)(double, double))
{
    double d1, d2;

    if (!PyArg_ParseTuple(args, "dd", &d1, &d2))
	return NULL;
    errno = 0;
    d1 = func(d1, d2);
    if (errno == EDOM) {
	PyErr_SetString(PyExc_ValueError, "math domain error");
	return NULL;
    } else if (errno == ERANGE) {
	PyErr_SetString(PyExc_OverflowError, "math range error");
	return NULL;
    }
    return Py_BuildValue("d", d1);
}

#define FNLIST(X,X2) \
    /* My functions */ \
    X(W, "The principal branch of the Lambert W function on the real\n" \
          "interval (-1/e,infinity).") \
    X(Wn, "The negative branch of the Lambert W function on the real\n" \
          "interval (-1/e,0).") \
    X(inverf, "The inverse of the error function.") \
    X(inverfc, "The inverse of the complementary error function.") \
    X(Phi, "The cumulative probability function of the standard normal\n" \
          "distribution.") \
    X(Phi0, "The cumulative probability function of the standard normal\n" \
          "distribution, integrated from zero rather than infinity.") \
    X(invPhi, "The inverse of the cumulative probability function of the\n" \
          "standard normal distribution.") \
    X(invPhi0, "The inverse of the cumulative probability function of the\n" \
          "standard normal distribution integrated from zero.") \
    /* C99 functions */ \
    X(erf, "The error function.") \
    X(erfc, "The complementary error function.") \
    X(tgamma, "The gamma function.") \
    X(lgamma, "The logarithm of the (absolute value of the) gamma function.") \
    X(exp2, "2^x.") \
    X(log2, "Logarithm to base 2.") \
    X(expm1, "exp(x)-1.") \
    X(log1p, "log(1+x).") \
    X(cbrt, "Cube root.") \
    X2(hypot, "Hypotenuse function sqrt(x^2+y^2).")

#define WRAPPER1(fn,desc) \
    static PyObject *python_ ## fn(PyObject *self, PyObject *args) \
	    { return python_wrapper1(self, args, fn); }
#define WRAPPER2(fn,desc) \
    static PyObject *python_ ## fn(PyObject *self, PyObject *args) \
	    { return python_wrapper2(self, args, fn); }
FNLIST(WRAPPER1,WRAPPER2)

#define METHOD(fn,desc) { #fn, python_ ## fn, METH_VARARGS, desc },
static PyMethodDef pymaths_methods[] = {
    FNLIST(METHOD,METHOD)
    {NULL, NULL, 0, NULL}
};

#ifdef PyMODINIT_FUNC
PyMODINIT_FUNC
#else
void
#endif
initpymaths(void)
{
    (void)Py_InitModule("pymaths", pymaths_methods);
}
