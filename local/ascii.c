#include <stdio.h>
#include <stdlib.h>

int main(int ac, char **av) {
    int i, j, full = 0;
    char *pname = *av;

    while (--ac) {
	char *p = *++av;
	if (!strcmp(p, "-f"))
	    full = 1;
	else {
	    fprintf(stderr, "usage: %s [-f]\n", pname);
	    exit (1);
	}
    }

    printf("   ");
    for (i=0; i<16; i++) printf(" %1X", i);
    printf("\n");
    for (i=2; i<8; i++) {
	printf("%1X0 ", i);
        for (j=0; j<(i<7?16:15); j++) printf(" %c", j+(i<<4));
	printf("\n");
    }
    if (full) {
	printf("\n");
	for (i=10; i<16; i++) {
	    printf("%1X0 ", i);
	    for (j=0; j<16; j++) printf(" %c", j+(i<<4));
	    printf("\n");
	}
    }
    return 0;
}
