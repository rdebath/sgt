/*
 * Switch control between various main() functions depending on
 * argv[0].
 */

#include <string.h>

extern int linuxrc_main(int argc, char **argv);
extern int nort_main(int argc, char **argv);
extern int sumo_main(int argc, char **argv);

int main(int argc, char **argv)
{
    char *arg, *c;

    if (argc == 0)
	arg = "";
    else
	arg = argv[0];
	
    c = strrchr(arg, '/');
    if (c == NULL)
	c = arg;
    else
	c++;

    if (!strcmp(c, "nort"))
	return nort_main(argc, argv);
    if (!strcmp(c, "sumo"))
	return sumo_main(argc, argv);

    return linuxrc_main(argc, argv);
}
