/*  Run unit tests for timber.  */

#include <check.h>
#include <stdlib.h>


Suite *main_suite (void)
{
    return suite_create ("main");
}


int main (void)
{
    int nf;
    SRunner *sr = srunner_create (main_suite());
    srunner_run_all (sr, CK_NORMAL);
    nf = srunner_ntests_failed (sr);
    srunner_free (sr);
    return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
