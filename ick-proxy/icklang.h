/*
 * icklang.h: API for an implementation of ick-proxy's toy string
 * processing language (called Ick, naturally). Contains functions
 * to load and parse a script, to execute it, and to translate it
 * into Javascript.
 */

#ifndef ICK_LANG_H
#define ICK_LANG_H

#include <stdio.h>
#include <stdarg.h>

/*
 * Types supported by the Ick language are string, int and bool.
 * Function prototypes are represented in this API by a string
 * consisting of the letters S, I, B and V: the first character of
 * the string specifies the function's return type, and the
 * remainder specify the parameters if any. (Only the return type
 * may be V.)
 */

typedef struct ickscript ickscript;    /* parsed and compiled script */
typedef struct icklib icklib;	       /* standard library */

/*
 * Read an Ick script from an arbitrary data source, and compile
 * it into its internal representation ready to be executed.
 * 
 * The script should define a specific function with a particular
 * prototype, which will be the one called when the script is
 * executed (and the ick_execute function will expect the right
 * arguments to be passed).
 * 
 * The `getch' function provides the script data. It returns as if
 * it were fgetc: an int containing an unsigned char value, or
 * negative for EOF.
 * 
 * `lib' should be a standard library structure created with
 * ick_lib_new() (and ick_lib_addfn() if you want to provide extra
 * library functions to the script). If it is NULL, the script
 * will have no library functions available at all.
 * 
 * The return value is a valid runnable script, unless there was a
 * compile-time error in which case NULL is returned and `err' is
 * filled in with a dynamically allocated error message.
 */
ickscript *ick_compile(char **err,
		       const char *mainfuncname, const char *mainfuncproto,
		       icklib *lib, int (*read)(void *ctx), void *ctx);
/*
 * Convenience versions of the above, pre-cooked to read from an
 * open stdio file or an in-memory zero-terminated string.
 */
ickscript *ick_compile_fp(char **err,
			  const char *mainfuncname, const char *mainfuncproto,
			  icklib *lib, FILE *fp);
ickscript *ick_compile_str(char **err,
			   const char *mainfuncname, const char *mainfuncproto,
			   icklib *lib, const char *script);

/*
 * Make an Ick standard library structure. If `empty' is false, it
 * will contain the standard functions (but you can then add
 * more). Otherwise, it will contain no functions at all.
 */
icklib *ick_lib_new(int empty);
/*
 * Register an extra function in the standard library. The
 * prototype has the same format as `mainfuncproto' above.
 * 
 * The function pointer type icklib_fn_t describes the prototype
 * that such a library function should provide. On entry, the
 * string parameters are read from `sparams', and the
 * integer/boolean parameters interleaved in `iparams'. For
 * functions returning int or boolean, the result should be
 * written to *(int *)result; for functions returning string, a
 * string should be dynamically allocated and written to *(char
 * **)result.
 * 
 * Library functions should return 1 for successful execution. If
 * they return 0, this is treated as a run-time exception and
 * script execution is immediately terminated.
 * 
 * `jstranslate' is an additional function pointer which is called
 * to generate a Javascript translation of a call to the function.
 * It is passed a list of strings (in `args') which contain
 * Javascript translations of the expressions giving the function
 * arguments; it must then generate the text of the function call
 * by writing it out to `ctx' via the provided `write' function.
 * `jstranslate' may be null, in which case ick_to_js will fail if
 * given a script calling that library function.
 */
typedef int (*icklib_fn_t)(void *result, const char **sparams,
			   const int *iparams);
typedef void (*icklib_jswrite_fn_t)(void *ctx, const char *buf);
typedef void (*icklib_js_fn_t)(icklib_jswrite_fn_t write, void *ctx,
			       const char **args);
void ick_lib_addfn(icklib *lib, const char *funcname, const char *funcproto,
		   icklib_fn_t func, icklib_js_fn_t jstranslate);
/*
 * Free a standard library structure. This will break any script
 * structure which was linked against it, so don't do it until
 * after you've finished running scripts.
 */
void ick_lib_free(icklib *lib);

/*
 * Execute an Ick script.
 *
 * The `result' parameter should point to a variable of the
 * correct type to contain the main function's result; that is, it
 * should be a char ** cast to void * if mainfuncproto began with
 * "S" when ick_compile was called, and otherwise it should be an
 * int * cast to void *. In the former case, the returned string
 * will be zero-terminated and newly dynamically allocated; free
 * it yourself with free() when you've finished with it.
 * 
 * If the main function has parameters, they should appear after
 * the explicit parameters, and must all be of the correct type
 * (either [const] char * or int).
 * 
 * The return value from ick_exec() is zero for successful
 * execution, or greater than zero for a run-time error (see enum
 * below). In the latter case the result will not have been filled
 * in.
 */
int ick_exec(void *result, ickscript *script, ...);
/*
 * In case it's useful, a variant form of the above taking an
 * explicit va_list.
 */
int ick_exec_v(void *result, ickscript *script, va_list ap);
/*
 * And versions of the above which also let you limit the number
 * of VM instructions executed, the stack depth, and the total
 * size of strings allocated on the stack.
 * 
 * If you pass one of these limits as zero, it leaves that
 * particular quantity unlimited.
 */
int ick_exec_limited(void *result, int maxcycles, int maxstk, int maxstr,
		     ickscript *script, ...);
int ick_exec_limited_v(void *result, int maxcycles, int maxstk, int maxstr,
		       ickscript *script, va_list ap);

/*
 * Enumeration of run-time error codes. The strange macro is so
 * that I can define the ick_runtime_errors[] error text array in
 * the accompanying source.
 * 
 * Values equal to or greater than ICK_RTE_USER are unused by the
 * Ick implementation, and are free for use by user library
 * functions.
 */
#define ICK_RTE_ERRORLIST(A) \
    A(ICK_RTE_NO_ERROR, "success"), \
    A(ICK_RTE_CYCLE_LIMIT_EXCEEDED, "script exceeded VM cycle limit"), \
    A(ICK_RTE_STACK_LIMIT_EXCEEDED, "script exceeded VM stack limit"), \
    A(ICK_RTE_STRINGS_LIMIT_EXCEEDED, "script exceeded VM strings limit"), \
    A(ICK_RTE_DIVBYZERO, "division by zero"), \
    A(ICK_RTE_SUBSTR_BACKWARDS, "substr() arg 3 was less than arg 2")
#define ICK_RTE_ENUM_DEF(x,y) x
enum { ICK_RTE_ERRORLIST(ICK_RTE_ENUM_DEF), ICK_RTE_USER };
#undef ICK_RTE_ENUM_DEF
extern const char *const ick_runtime_errors[];

/*
 * Convert an Ick script into a JavaScript fragment.
 * 
 * The return value is a piece of (hopefully) valid JavaScript
 * which is approximately semantically equivalent to the input
 * script. (`Approximately' meaning, in particular, that no
 * attention is paid to difficult cases of integer overflow. This
 * is a language for simple string processing; you shouldn't be
 * trying to do anything complicated with large integers in it.)
 * 
 * `avoidids' should be an array of JavaScript identifier names to
 * avoid using; `navoidids' gives the number of entries in the
 * array.
 * 
 * `mainfunc' points to a `char *' which in turn points to the
 * identifier of the JavaScript function corresponding to the main
 * function of the script. If you want to specify this identifier
 * yourself on input, set *mainfunc to non-NULL; otherwise, set
 * *mainfunc to NULL and ick_to_js() will invent its own identifer
 * and return it. In the latter case, the new value of *mainfunc
 * will be dynamically allocated and you are responsible for
 * freeing it.
 * 
 * In all cases, the JavaScript fragment returned from ick_to_js()
 * is dynamically allocated.
 */
char *ick_to_js(char **mainfunc, ickscript *script,
		const char **avoidids, int navoidids);

/*
 * Free an Ick script structure.
 */
void ick_free(ickscript *script);

#endif /* ICK_LANG_H */
