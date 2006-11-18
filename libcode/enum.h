/*
 * Library of reusable functions to handle permutations and
 * combinations.
 */

/* ----------------------------------------------------------------------
 * Functions which simply return the total number of a given type
 * of thing.
 */

/*
 * Return the total number of possible permutations of n items,
 * i.e. n!.
 */
int perm_count(int n);

/*
 * Return the total number of possible (n,k) combinations, i.e. the
 * binomial coefficient n!/(k!(n-k)!).
 */
int comb_count(int n, int k);

/* ----------------------------------------------------------------------
 * Functions to handle permutations of n items.
 * 
 * In all the functions in this section, `array' is presumed to
 * point to an array of n ints, with array[i] giving the index of
 * the ith element of the represented permutation. When this array
 * is taken as input, the functions merely expect that its n
 * elements are distinct; on output, perm_from_index and perm_first
 * guarantee that it contains 0..n-1 in some order, whereas
 * perm_next guarantees that the output array is a permutation of
 * the input one.
 */

/*
 * Given a permutation, return the next permutation in
 * lexicographic order. The ordering is the intuitive one: the
 * lexicographically smallest permutation is the one with the
 * elements in natural order (0,...,n-1), and the largest the one
 * with the elements in reverse order (n-1,...,0).
 * 
 * Return value is false if no larger combination exists.
 */
int perm_next(int *array, int n);

/*
 * Write out the smallest permutation, as defined above: the one
 * with the elements in natural order. This function is trivial,
 * but it goes well with perm_next() because it allows
 * iterations such as
 * 
 *   perm_first(array, n);
 *   do {
 *       // stuff
 *   } while (perm_next(array, n));
 */
void perm_first(int *array, int n);

/*
 * Find the numeric index of a permutation in the above
 * lexicographic order.
 */
int perm_to_index(int *array, int n);

/*
 * Restore a permutation from its numeric index.
 */
void perm_from_index(int *array, int n, int index);

/*
 * FIXME: provide a permutation-inversion function.
 */

/*
 * FIXME: provide a permutation parity measurer.
 */

/* ----------------------------------------------------------------------
 * Functions to handle combinations (size-k subsets of a size-n
 * set), represented as sets. All functions in this section expect
 * that 0 <= k <= n, and that (n choose k) is not within a factor
 * of n of INT_MAX.
 *
 * In all the functions in this section, `array' is presumed to
 * point to an array of n chars, with array[i] non-zero iff i is
 * part of the represented set. When this array is taken as input,
 * the functions expect that exactly k of its n elements are
 * non-zero; when it is given as output, the functions guarantee
 * the same provided the guarantee was preserved on input.
 */

/*
 * Given an (n,k) combination, return the next (n,k) combination in
 * lexicographic order. Unset bits are assumed to sort _after_ set
 * bits, so that the lexicographically smallest combination is the
 * one with the first k bits set, and the largest the one with the
 * last k bits set. (This is exactly counterintuitive if you're
 * considering the set to be a big-endian binary number, but it has
 * the useful property that it progresses through (n,1)
 * combinations from first to last and is generally intuitive in
 * most other respects. Also, it matches the numcomb_*() order.)
 * 
 * Return value is false if no larger combination exists.
 */
int setcomb_next(char *array, int n, int k);

/*
 * Write out the smallest (n,k) combination, as defined above: the
 * one with the first k elements set. This function is trivial, but
 * it goes well with setcomb_next() because it allows iterations
 * such as
 * 
 *   setcomb_first(array, n, k);
 *   do {
 *       // stuff
 *   } while (setcomb_next(array, n, k));
 */
void setcomb_first(char *array, int n, int k);

/*
 * Find the numeric index of an (n,k) combination in the above
 * lexicographic order.
 */
int setcomb_to_index(char *array, int n, int k);

/*
 * Restore a combination from its numeric index.
 */
void setcomb_from_index(char *array, int n, int k, int index);

/* ----------------------------------------------------------------------
 * Functions to handle combinations again, but this time
 * represented as a list of the selected items in increasing order.
 * All functions in this section expect that 0 <= k <= n, and that
 * (n choose k) is not within a factor of n of INT_MAX.
 * 
 * In all the functions in this section, `array' is assumed to
 * point to an array of k ints, with array[i] giving the index of
 * the ith smallest selected item. When this array is taken as
 * input, the functions expect that its k elements are distinct, in
 * increasing order, and in the range [0,...,n-1]; when it is given
 * as output, the functions guarantee the same provided the
 * guarantee was preserved on input.
 */

/*
 * Given an (n,k) combination, return the next (n,k) combination in
 * lexicographic order. Smaller-numbered elements are assumed to
 * sort below larger-numbered ones, so that the lexicographically
 * smallest combination is (0,...,k-1) and the largest is
 * (n-k,...,n-1).
 * 
 * Return value is false if no larger combination exists.
 */
int listcomb_next(int *array, int n, int k);

/*
 * Write out the smallest (n,k) combination, as defined above: the
 * one with the first k elements set. This function is trivial, but
 * it goes well with listcomb_next() because it allows iterations
 * such as
 * 
 *   listcomb_first(array, n, k);
 *   do {
 *       // stuff
 *   } while (listcomb_next(array, n, k));
 */
void listcomb_first(int *array, int n, int k);

/*
 * Find the numeric index of an (n,k) combination in the above
 * lexicographic order.
 */
int listcomb_to_index(int *array, int n, int k);

/*
 * Restore a combination from its numeric index.
 */
void listcomb_from_index(int *array, int n, int k, int index);
