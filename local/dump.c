#include <stdio.h>
#include <stdlib.h>

int lower=0, squash=1, ascii=1;
long offset=0, start=0, linewidth=16;

void help(void) {
    static char *helptext[] = {
	"usage: dump [-afhl] [-o offset] [-s start] [-w width]"
	    " [file [file...]]\n",
	"       -a disables ASCII-mode display\n",
	"       -f forces full display (disables `etc...' lines)\n",
	"       -h displays this text\n",
	"       -l displays hex numbers in lower case\n",
	"       -o ensures that a new line starts <offset> bytes into"
	    " the file\n",
	"       -s adds <start> to all the displayed offsets\n",
	"       -w chooses how many bytes to dump per display line\n",
	NULL
    };
    char **p = helptext;

    while (*p)
	fputs (*p++, stderr);
}

main(int argc, char **argv)
{
    FILE *fp;
    int i, j;
    int flag;
    char **names;

    flag = 0;
    names = malloc(argc*sizeof(*names));
    if (!names) {
	perror("malloc");
	return 1;
    }

    j = 0;
    while (--argc > 0) {
	char c, *v, *p = *++argv;
        if (*p == '-') {
	    if (!strcmp(p, "--help")) {
		help();
		return 0;
	    }
	    p++;
	    while ( (c = *p++) ) switch (c) {
	      case 'l':		       /* lower case */
		lower = 1;
		break;
	      case 'f':		       /* force full display */
		squash = 0;
		break;
	      case 'a':		       /* disable ascii display */
		ascii = 0;
		break;
	      case 'h':		       /* help text */
	      case '?':
		help();
		return 0;
	      case 'o':		       /* options with arguments */
	      case 's':
	      case 'w':
		if (*p)
		    v = p, p = "";
		else if (--argc > 0)
		    v = *++argv;
		else {
		    fprintf(stderr,
			    "dump: option `-%c' requires an argument\n", c);
		    return 1;
		}
		switch (c) {
		  case 'o':
		    offset = strtoul(v,NULL,0);
		    break;
		  case 's':
		    start = strtoul(v,NULL,0);
		    break;
		  case 'w':
		    linewidth = strtoul(v,NULL,0);
		    break;
		}
		break;
	      default:
		fprintf(stderr, "dump: unrecognised option `-%c'\n", c);
		help();
		return 1;
	    }
	} else
	    names[j++] = p;
    }

    offset=offset%linewidth;

    for (i = 0; i < j; i++)
    {
	flag=1;
	fp = fopen (names[i], "r");
	if (fp == NULL)
	    printf ("dump: Couldn't open file %s\n", names[i]);
	else
	{
	    dump (fp);
	    fclose (fp);
	}
    }
    if (!flag)
	dump (stdin);
}	    

dump (FILE *fp)
{   
    long addr=0;
    int i, j, c, flag=1;

    unsigned char hex[16]="0123456789abcdef";
    unsigned char *buffer;
    unsigned char *pbuf;
    unsigned char *prl, *oldprl;
    char *fmt = lower ? "%08lx  %s\n" : "%08lX  %s\n" ;
    int g, etcflag = 0;
    int byteoffset;
    
    buffer = malloc (linewidth);
    pbuf = malloc (linewidth+1);
    prl = malloc (linewidth*4+17); /* Should be enough :) */
    oldprl = malloc (linewidth*4+17);
    if (!buffer || !pbuf || !prl || !oldprl)
    {
	fprintf (stderr, "Out of memory.\n");
	exit (1);
    }
    if (!lower)
	for (i = 10;i<16;i++)
	    hex[i]=hex[i]&95;
    if (offset)
	byteoffset=linewidth-offset;
    else
	byteoffset=0;
    while (flag)
    {
	i=byteoffset;
	while (flag && i<linewidth)
	{
	    g = fgetc(fp);
	    if (g == EOF)
		flag=0;
	    else
	    {
		buffer[i]=g;
		i++;
	    }
	}
	if (i)
	{
	    for (c = 0; c < linewidth*3+3; c++)
		prl[c]=' ';
	    for (j = byteoffset; j < i; j++)
	    {
		prl[j*3]=hex[buffer[j]/16];
		prl[j*3+1]=hex[buffer[j]%16];
	    }
	    if (ascii) {
		prl[i+linewidth*3+1]='\0';
		for (j=0; j < byteoffset; j++)
		    pbuf[j]=' ';
		for (j = byteoffset; j < i; j++)
		{
		    if (31<buffer[j] && 127>buffer[j])
			prl[j+linewidth*3+1]=buffer[j];
		    else
			prl[j+linewidth*3+1]='.';
		}
	    } else
		prl[3*i-1] = '\0';

	    if ( (c = fgetc(fp)) != EOF )
		ungetc (c, fp) ;

	    if (squash && !strcmp(oldprl, prl) && !feof(fp)) {
		if (!etcflag) {
		    etcflag = 1 ;
		    printf("etc...\n") ;
		}
	    } else {
		etcflag = 0 ;
		printf (fmt, addr+start, prl);
		strcpy (oldprl, prl) ;
	    }
	    addr+=linewidth-byteoffset;
	}
	byteoffset=0;
    }
    free (buffer);
    free (pbuf);
}
