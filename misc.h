/* ----------------------------------------------------------------------
 * Generally useful declarations.
 */

#ifndef MISC_H
#define MISC_H

#define TRUE 1
#define FALSE 0
#define lenof(x) (sizeof ((x)) / sizeof ( *(x) ))

struct Size {
    int w, h;
};

struct RGB {
    double r, g, b;
};

#endif /* MISC_H */
