#include <stdio.h>
#include <fcntl.h>

static void hex(char *, char *, int);

int main(int ac, char **av) {
    int fd, len, i;
    static char buffer[2048];

    if (ac!=3) {
	fprintf(stderr, "usage: %s <bootsector> <loader>\n", *av);
	exit(0);
    }

    if ( (fd = open(av[1], O_RDONLY)) < 0) {
	perror(av[1]);
	exit(1);
    }
    len = read (fd, buffer, sizeof(buffer));
    for (i=0; i<len-5; i++)
	if (!strncmp(buffer+i, "DBOOT", 5))
	    break;
    if (i>=len-5) {
	fprintf(stderr, "%s: does not contain \"DBOOT\" signature\n", av[1]);
	exit(1);
    }
    printf("int bs_data_offset = %d;\n", i+5);
    close(fd);
    hex("bs_data", buffer, len);
    putchar('\n');

    if ( (fd = open(av[2], O_RDONLY)) < 0) {
	perror(av[2]);
	exit(1);
    }
    len = read (fd, buffer, sizeof(buffer));
    len -= 4;
    while (len>0 && strncmp(buffer+len, "____", 4))
	len--;
    if (len==0) {
	fprintf(stderr, "%s: does not contain \"____\" signature\n", av[2]);
	exit(1);
    }
    close(fd);
    hex("ld_data", buffer, len);

    return 0;
}

static void hex(char *name, char *buffer, int len) {
    int col = 0;
    char *p = buffer;

    printf("int %s_len = %d;\nunsigned char %s[] = {\n", name, len, name);
    while (len--) {
	char x[10];
	printf("%s 0x%02X%s", (col ? "" : "   "), (unsigned char) *p++,
	       (len ? "," : ""));
	col++;
	if (col==12) {
	    col=0;
	    printf("\n");
	}
    }
    printf("%s};\n", (col ? "\n" : ""));
}
