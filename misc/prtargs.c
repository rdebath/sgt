#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int i;
    printf("argc = %d\n", argc);
    for (i = 0; i < argc; i++)
        printf("argv[%d] (%u chars) = '%s'\n", i,
               (unsigned)strlen(argv[i]), argv[i]);
    return 0;
}
