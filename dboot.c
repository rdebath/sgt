/* dboot.c  Install DOS-partition Boot Loader (DBOOT) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <linux/hdreg.h>
#include <linux/major.h>

#define TRUE 1
#define FALSE 0

#define CONF_LEN 8		       /* length of config entry */

enum SECT {
    NONE = 0, BIOS = 0x8000, DBOOT = 0x8001
};

extern int bs_data_offset;	       /* in "data.c" */
extern int bs_data_len;		       /* autogenerated */
extern unsigned char bs_data[];	       /* by "makeloader" */
extern int ld_data_len;
extern unsigned char ld_data[];

static void parse_cmdline(int, char **);
static void dump_data(void);
static void read_cfg(void);
static void do_write(int);
static void do_directive (enum SECT, char *, char *);
static void end_sect(enum SECT);
static void dos_parse_fname(char *, char *);
static char *getline (FILE *);

static char cfg_name[512] = "/etc/dboot.ini";
static char *pname;
static dev_t bios_dev_map[256];	       /* BIOS -> Linux device map */
static int heads[256], sectors[256];   /* disk geometry for above */
static int bdev_bios = -1, bdev_start; /* boot device parameters */
static char bdev_dir[512] = "";	       /* where is boot dev mounted? */
static char message[512] = "";	       /* boot prompt */
static char bsector_name[512] = "";    /* where to store boot sector */
static int mbr = -1;		       /* is it an MBR? */
static char dosloader[14], loader[512] = "";/* boot loader file */
static int options = 0;		       /* # of boot options */
static int nodefault = 0;	       /* load first option, by default */
static struct option {
    char name[512];		       /* option name */
    char keys[16];		       /* key strokes */
    enum {NOTYPE, KERN=1, BSECT=2, BFILE=3} type; /* type */
    union {
	struct ktype {
	    char dosname[14];	       /* kernel file DOS name */
	    char cmdline[2048];	       /* command line */
	} k;
	struct btype {
	    int mbr_cx, mbr_dx;
	    int bs_cx, bs_dx;
	    int si;
	    unsigned short scans[16];  /* scancodes to insert */
	} b;
	struct bftype {
	    char dosname[14];	       /* bootsector file DOS name */
	    unsigned short scans[16];  /* scancodes to insert */
	} bf;
    } u;
} opts[16];			       /* hell, why link a list? :) */
static int verbose = 0;
static int test = 0;

struct fk {
    char *name;
    int code;
};
static struct fk fknames[] = {
{"shifttab",15}, {"altq",16}, {"altw",17}, {"alte",18}, {"altr",19},
{"altt",20}, {"alty",21}, {"altu",22}, {"alti",23}, {"alto",24},
{"altp",25}, {"alta",30}, {"alts",31}, {"altd",32}, {"altf",33},
{"altg",34}, {"alth",35}, {"altj",36}, {"altk",37}, {"altl",38},
{"altz",44}, {"altx",41}, {"altc",42}, {"altv",43}, {"altb",44},
{"altn",45}, {"altm",46}, {"f1",59}, {"f2",60}, {"f3",61}, {"f4",62},
{"f5",63}, {"f6",64}, {"f7",65}, {"f8",66}, {"f9",67}, {"f10",68},
{"home",71}, {"up",72}, {"pgup",73}, {"left",75}, {"right",77},
{"end",79}, {"down",80}, {"pgdn",81}, {"ins",82}, {"del",83},
{"shiftf1",84}, {"shiftf2",85}, {"shiftf3",86}, {"shiftf4",87},
{"shiftf5",88}, {"shiftf6",89}, {"shiftf7",90}, {"shiftf8",91},
{"shiftf9",92}, {"shiftf10",93}, {"ctrlf1",94}, {"ctrlf2",95},
{"ctrlf3",96}, {"ctrlf4",97}, {"ctrlf5",98}, {"ctrlf6",99},
{"ctrlf7",100}, {"ctrlf8",101}, {"ctrlf9",102}, {"ctrlf10",103},
{"altf1",104}, {"altf2",105}, {"altf3",106}, {"altf4",107},
{"altf5",108}, {"altf6",109}, {"altf7",110}, {"altf8",111},
{"altf9",112}, {"altf10",113}, {"ctrlprtsc",114}, {"ctrlleft",115},
{"ctrlright",116}, {"ctrlend",117}, {"ctrlpgdn",118},
{"ctrlhome",119}, {"alt1",120}, {"alt2",121}, {"alt3",122},
{"alt4",123}, {"alt5",124}, {"alt6",125}, {"alt7",126}, {"alt8",127},
{"alt9",128}, {"alt0",129}, {"altminus",130}, {"altequal",131},
{"ctrlpgup",132}, {"f11",133}, {"f12",134}, {"shiftf11",135},
{"shiftf12",136}, {"ctrlf11",137}, {"ctrlf12",138}, {"altf11",139},
{"altf12",140} };

int main(int ac, char **av) {
    pname = *av;
    parse_cmdline(ac, av);
    read_cfg();
    if (verbose)
	dump_data();
    if (test != 1)
	do_write(test);
    return 0;
}

/* Parse the command line */
static void parse_cmdline(int ac, char **av) {
    while (--ac) {
	char *q, *p = *++av;
	if (*p != '-') {
	    fprintf(stderr, "usage: %s [-t] [-T] [-v] [-f filename]\n", pname);
	    exit(0);
	}
	while (*++p) switch (*p) {
	  case '?': case 'h': case 'H':
	    fprintf(stderr, "usage: %s [-t] [-T] [-v] [-f filename]\n", pname);
	    exit(0);
	    break;		       /* force of habit */
	  case 't':
	    test = 1;		       /* just evaluate config */
	    break;
	  case 'T':
	    test = 2;		       /* dump bsect/loader to files */
	    break;
	  case 'v':		       /* verbose mode on */
	    verbose = 1;
	    break;
	  case 'f':		       /* change config file name */
	    if (p[1])
		q = p+1;
	    else if (ac>1)
		--ac, q = *++av;
	    else {
		fprintf(stderr, "%s: option -f requires a file name\n",
			pname);
		exit(1);
	    }
	    strcpy (cfg_name, q);
	    p="\0";		       /* not just "" - it gets incremented */
	    break;
	}
    }
}

/* Dump all the data gleaned from the config file to stdout */
static void dump_data(void) {
    int i;
    printf("\nBIOS drive mappings:\n");
    for (i=0; i<256; i++) {
	if (bios_dev_map[i]) {
	    printf("0x%02X: Linux device 0x%04X, %d heads, %d sectors\n",
		   i, bios_dev_map[i], heads[i], sectors[i]);
	}
    }

    printf("\nBoot device: BIOS 0x%02X, %d heads, %d sectors, start %d,"
	   " mounted on %s\n",
	   bdev_bios, heads[bdev_bios], sectors[bdev_bios], bdev_start,
	   bdev_dir);
    printf("Loader file = \"%s\" (\"%s\")\n", loader, dosloader);
    printf("Boot sector = \"%s\" (%s)\n", bsector_name,
	   mbr ? "MBR" : "not MBR");
    printf("Boot prompt = \"%s\"\n", message);

    for (i=0; i<options; i++) {
	char *k;

	printf("\nBoot option %d (%s):\n  keys:", i, opts[i].name);
	k = opts[i].keys;
	while (*k)
	    printf((*k>=' ' && *k<='~' ? " '%c'" : " %d"), *k), k++;
	printf("\n  type: %d", opts[i].type);
	switch (opts[i].type) {
	  case KERN:
	    printf(" (kernel)\n  filename: \"%s\"\n  command line: \"%s\"\n",
		   opts[i].u.k.dosname, opts[i].u.k.cmdline);
	    break;
	  case BFILE:
	    printf(" (bootfile)\n  filename: \"%s\"\n",
		   opts[i].u.bf.dosname);
	    if (opts[i].u.bf.scans[0]) {
		unsigned short *sp = opts[i].u.bf.scans;
		printf("  scancodes:");
		while (*sp)
		    printf(" 0x%04X", *sp++);
		printf("\n");
	    }
	    break;
	  case BSECT:
	    printf(" (bootsector)\n  device CX/DX: %04X/%04X\n",
		   opts[i].u.b.bs_cx, opts[i].u.b.bs_dx);
	    if (opts[i].u.b.mbr_cx != -1)
		printf("  MBR CX/DX: %04X/%04X, SI=%04X\n",
		       opts[i].u.b.mbr_cx, opts[i].u.b.mbr_dx,
		       opts[i].u.b.si);
	    if (opts[i].u.b.scans[0]) {
		unsigned short *sp = opts[i].u.b.scans;
		printf("  scancodes:");
		while (*sp)
		    printf(" 0x%04X", *sp++);
		printf("\n");
	    }
	    break;
	}
    }
}

/* Read the config file */
static void read_cfg(void) {
    char *p, *q;
    int i, bof;
    enum SECT sect = 0, boots = 0;
    FILE *fp = fopen (cfg_name, "r");

    if (!fp) {
	fprintf(stderr, "%s: unable to open configuration file \"%s\"\n",
		pname, cfg_name);
	exit(1);
    }

    if (verbose)
	printf("Reading config file %s...\n", cfg_name);

    /* init the structures */
    for (i=0; i<256; i++)
	bios_dev_map[i] = 0;	       /* no device mapped */

    /* get the first section header */
    p = getline(fp);
    if (!p) {
	fprintf(stderr, "%s: configuration file \"%s\" is empty\n",
		pname, cfg_name);
	exit(1);
    }
    if (strlen(p)==1 || *p!='[' || p[strlen(p)-1]!=']') {
	fprintf(stderr, "%s: configuration file \"%s\" does not begin"
		" with section header\n", pname, cfg_name);
	exit(1);
    }

    bof = TRUE;
    /* parse a section: beginning with the section header in p */
    while (*p) {
	if (!strcasecmp(p, "[BIOS]")) {
	    if (!bof) {
		fprintf(stderr, "%s: [BIOS] section must appear "
			"at start of file\n", pname);
		exit(1);
	    }
	    sect = BIOS;
	} else if (!strcasecmp(p, "[DBOOT]"))
	    sect = DBOOT;
	else {
	    sect = ++boots;
	    p[strlen(p)-1] = 0;
	    strcpy (opts[options].name, p+1);
	    opts[options].type = NOTYPE;
	    opts[options].keys[0] = 0;
	}
	if (bof && sect != BIOS) {
	    do_directive (BIOS, "0", "/dev/fd0");
	    do_directive (BIOS, "128", "/dev/hda");
	}
	bof = FALSE;
	/* read the actual section */
	while (1) {
	    p = getline (fp);
	    if (!p) {
		end_sect (sect);
		p = "";		       /* signal end of file */
		break;
	    }
	    if (*p=='[' && p[strlen(p)-1]==']') {
		end_sect (sect);
		break;		       /* next section header */
	    }
	    if ( (q = strchr(p, '=')) == NULL ) {
		fprintf(stderr, "%s: unable to parse line \"%s\" in"
			" configuration file \"%s\"\n", pname, p,
			cfg_name);
		exit(1);
	    }
	    *q++ = '\0';
	    while (strlen(p)>0 && isspace(p[strlen(p)-1]))
		p[strlen(p)-1] = '\0';
	    while (isspace(*q)) q++;
	    if (!*p || !*q) {
		fprintf(stderr, "%s: unable to parse line \"%s\" in"
			" configuration file \"%s\"\n", pname, p,
			cfg_name);
		exit(1);
	    }
	    do_directive (sect, p, q);
	}
    }
}

/* Parse a directive */
static void do_directive (enum SECT sect, char *token, char *value) {
    if (sect == BIOS) {
	/* the only directives here take the form
	 * <number> = <Linux device filename>
	 * or maybe
	 * <number>heads = <number>
 	 * <number>sectors = <number> */
	int fd, drive = 0;
	struct stat st;
	struct hd_geometry hdprm;

	while (isdigit(*token))
	    drive = drive*10 + (*token++ - '0');
	if (drive<0 || drive>255) {
	    fprintf(stderr, "%s: unrecognised [BIOS] token \"%s\" in"
		    " configuration file \"%s\"\n", pname, token,
		    cfg_name);
	    exit(1);
	}

	if (!*token) {
	    if (stat (value, &st) < 0) {
		fprintf(stderr, "%s: stat \"%s\": %s\n", pname, value,
			strerror(errno));
		exit(1);
	    }
	    if (st.st_rdev == 0) {
		fprintf(stderr, "%s: %s is not a device\n", pname, value);
		exit(1);
	    }
	    if (bios_dev_map[drive] != 0) {
		fprintf(stderr, "%s: BIOS drive %d already mapped\n", pname,
			drive);
		exit(1);
	    }
	    bios_dev_map[drive] = st.st_rdev;
	    if (MAJOR(st.st_rdev) != FLOPPY_MAJOR) {
		if ( (fd = open(value, O_RDONLY)) < 0) {
		    fprintf(stderr, "%s: open \"%s\": %s\n", pname, value,
			    strerror(errno));
		    exit(1);
		}
		if (ioctl (fd, HDIO_GETGEO, &hdprm) < 0) {
		    fprintf(stderr, "%s: ioctl HDIO_GETGEO \"%s\": %s\n",
			    pname, value, strerror(errno));
		    exit(1);
		} else {
		    if (hdprm.start != 0) {
			fprintf(stderr, "%s: \"%s\": is a partition\n", pname,
				value);
			exit(1);
		    }
		    heads[drive] = hdprm.heads;
		    sectors[drive] = hdprm.sectors;
		}
		close (fd);
	    } else {
		heads[drive] = sectors[drive] = 1;   /* floppy - matters not */
	    }
	} else if (!strcmp(token, "heads")) {
	    if (bios_dev_map[drive] == 0) {
		fprintf(stderr, "%s: BIOS drive %d not mapped\n", pname,
			drive);
		exit(1);
	    }
	    heads[drive] = atoi(value);
	} else if (!strcmp(token, "sectors")) {
	    if (bios_dev_map[drive] == 0) {
		fprintf(stderr, "%s: BIOS drive %d not mapped\n", pname,
			drive);
		exit(1);
	    }
	    sectors[drive] = atoi(value);
	} else {
	    fprintf(stderr, "%s: unrecognised [BIOS] token \"%s\" in"
		    " configuration file \"%s\"\n", pname, token,
		    cfg_name);
	    exit(1);
	}
    } else if (sect == DBOOT) {
	if (!strcasecmp(token, "MBR")) {
	    if (!strcasecmp(value, "TRUE")) {
		mbr = TRUE;
	    } else if (!strcasecmp(value, "FALSE")) {
		mbr = FALSE;
	    } else {
		fprintf(stderr,
			"%s: \"mbr\" must be \"true\" or \"false\"\n",
			pname);
		exit(1);
	    }
	} else if (!strcasecmp(token, "BOOTSECTOR")) {
	    strcpy (bsector_name, value);
	} else if (!strcasecmp(token, "PARTITION")) {
	    int fd;
	    struct stat st;
	    struct hd_geometry hdprm;
	    dev_t dev;
	    int i;

	    if (stat (value, &st) < 0) {
		fprintf(stderr, "%s: stat \"%s\": %s\n", pname, value,
			strerror(errno));
		exit(1);
	    }
	    if (st.st_rdev == 0) {
		fprintf(stderr, "%s: %s is not a device\n", pname, value);
		exit(1);
	    }
	    dev = st.st_rdev & 0xFFF0; /* get "parent" device */
	    for (i=0; i<256; i++)
		if (dev == bios_dev_map[i])
		    break;
	    if (i==256) {	       /* not found */
		fprintf(stderr, "%s: \"%s\" (on %04X) not mapped to a"
			" BIOS drive\n", pname, value, dev);
		exit(1);
	    }
	    bdev_bios = i;
	    if ( (fd = open (value, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: open \"%s\": %s\n", pname, value,
			strerror(errno));
		exit(1);
	    }
	    if (ioctl (fd, HDIO_GETGEO, &hdprm) < 0) {
		fprintf(stderr, "%s: ioctl HDIO_GETGEO \"%s\": %s\n",
			pname, value, strerror(errno));
		exit(1);
	    }
	    close (fd);
	    bdev_start = hdprm.start;
	} else if (!strcasecmp(token, "MOUNTED")) {
	    strcpy (bdev_dir, value);
	} else if (!strcasecmp(token, "LOADER")) {
	    dos_parse_fname (dosloader, value);
	    strcpy (loader, value);
	} else if (!strcasecmp(token, "MESSAGE")) {
	    char *p, *q;

	    if (*value != '"' && value[strlen(value)-1] != '"') {
		fprintf(stderr, "%s: \"message\" line in [DBOOT] section"
			" has no quotes\n", pname);
		exit(1);
	    }
	    q = message + strlen(message);
	    p = value+1;
	    p[strlen(p)-1] = '\0';     /* eat trailing quote */
	    while (*p) {
		if (*p != '\\')
		    *q++ = *p;
		else {
		    p++;
		    switch (*p) {
		      case '\0': *q++ = '\\'; break;
		      case 'n': *q++ = '\r'; *q++ = '\n'; break;
		      case 'N': *q++ = '\n'; break;
		      case 'r': *q++ = '\r'; break;
		      case 'a': *q++ = '\a'; break;
		      default: *q++ = *p; break;
		    }
		}
		p++;
		if (q >= message+sizeof(message))   /* limit check */
		    q = message+sizeof(message)-1;   /* reset to end */
		*q = '\0';
	    }
	} else if (!strcasecmp(token, "DEFAULT")) {
	    if (!strcasecmp(value, "OFF"))
		nodefault = 1;
	    else if (!strcasecmp(value, "ON"))
		nodefault = 0;
	    else {
		fprintf(stderr, "%s: \"DEFAULT=\" line in [DBOOT] section"
			" should read OFF or ON\n", pname);
		exit(1);
	    }
	} else {
	    fprintf(stderr, "%s: unrecognised [BIOS] token \"%s\" in"
		    " configuration file \"%s\"\n", pname, token,
		    cfg_name);
	    exit(1);
	}
    } else {
	if (!strcasecmp(token, "KEYS")) {
	    char *k = opts[options].keys;
	    do {
		if (!*value) {
		    fprintf(stderr, "%s: badly formatted \"keys\" line "
			    "in boot option \"%s\"\n", pname,
			    opts[options].name);
		    exit(1);
		}
		if (value[1]==',' || value[1]=='\0') {
		    *k++ = *value++;
		} else {
		    int i = 0;
		    while (isdigit(*value))
			i = i*10 + (*value++ - '0');
		    *k++ = i;
		}
		if (*value && *value != ',') {
		    fprintf(stderr, "%s: badly formatted \"keys\" line "
			    "in boot option \"%s\"\n", pname,
			    opts[options].name);
		    exit(1);
		}
		if (*value)
		    value++;
	    } while (*value);
	    *k = 0;
	} else if (!strcasecmp(token, "TYPE")) {
	    if (!strcasecmp(value, "KERNEL")) {
		opts[options].type = KERN;
		opts[options].u.k.dosname[0] = 0;
		opts[options].u.k.cmdline[0] = 0;
	    } else if (!strcasecmp(value, "BOOTSECTOR")) {
		opts[options].type = BSECT;
		opts[options].u.b.mbr_cx = -1;
		opts[options].u.b.mbr_dx = -1;
		opts[options].u.b.bs_cx = -1;
		opts[options].u.b.bs_dx = -1;
		opts[options].u.b.si = -1;
		opts[options].u.b.scans[0] = 0;
	    } else if (!strcasecmp(value, "BOOTFILE")) {
		opts[options].type = BFILE;
		opts[options].u.bf.dosname[0] = 0;
		opts[options].u.bf.scans[0] = 0;
	    } else {
		fprintf(stderr, "%s: invalid type for boot option \"%s\"\n",
			pname, opts[options].name);
		exit(1);
	    }
	} else switch (opts[options].type) {
	  case KERN:
	    if (!strcasecmp(token, "FILENAME")) {
		dos_parse_fname (opts[options].u.k.dosname, value);
	    } else if (!strcasecmp(token, "CMDLINE")) {
		if (*value != '"' && value[strlen(value)-1] != '"') {
		    fprintf(stderr, "%s: \"cmdline\" line in boot option"
			    " \"%s\" has no quotes\n", pname,
			    opts[options].name);
		    exit(1);
		}
		/* FIXME: parse quoted text */
		value[strlen(value)-1] = 0;
		strcat (opts[options].u.k.cmdline, value+1);
	    } else {
		fprintf(stderr, "%s: unrecognised token \"%s\" in boot"
			" option \"%s\"\n", pname, token,
			opts[options].name);
		exit(1);
	    }
	    break;
	  case BFILE:
	    if (!strcasecmp(token, "FILENAME")) {
		dos_parse_fname (opts[options].u.bf.dosname, value);
	    } else if (!strcasecmp(token, "INSERT")) {
		unsigned short *sp = opts[options].u.bf.scans;
		do {
		    if (!*value) {
			fprintf(stderr, "%s: badly formatted \"insert\" line "
				"in boot option \"%s\"\n", pname,
				opts[options].name);
			exit(1);
		    }
		    if (value[1]==',' || value[1]=='\0') {
			*sp++ = (*value ? *value : 0x300);
			value++;
		    } else if (isdigit(*value)) {
			int i = 0;
			while (isdigit(*value))
			    i = i*10 + (*value++ - '0');
			*sp++ = (i ? i : 0x300);
		    } else { /* parse as function key */
			struct fk *f;
			int len = strcspn(value, ",");
			*sp = 0;
			for (f=fknames;
			     f<fknames+sizeof(fknames)/sizeof(*fknames);
			     f++)
			    if (!strncasecmp(f->name, value, len)) {
				*sp = f->code << 8;
				break;
			    }
			if (!*sp) {
			    fprintf(stderr, "%s: string \"%s\" does not "
				    "correspond to a known function key\n",
				    pname, value);
			    exit(1);
			}
			sp++;
			value += len;
		    }
		    if (*value && *value != ',') {
			fprintf(stderr, "%s: badly formatted \"insert\" line "
				"in boot option \"%s\"\n", pname,
				opts[options].name);
			exit(1);
		    }
		    if (*value)
			value++;
		} while (*value);
		*sp = 0;
	    } else {
		fprintf(stderr, "%s: unrecognised token \"%s\" in boot"
			" option \"%s\"\n", pname, token,
			opts[options].name);
		exit(1);
	    }
	    break;
	  case BSECT:
	    if (!strcasecmp(token,"DEVICE") || !strcasecmp(token,"TABLE")) {
		int fd;
		struct stat st;
		struct hd_geometry hdprm;
		dev_t dev;
		int i, bios, offset, sector, head, cyl, cx, dx;

		if (stat (value, &st) < 0) {
		    fprintf(stderr, "%s: stat \"%s\": %s\n", pname, value,
			    strerror(errno));
		    exit(1);
		}
		if (st.st_rdev == 0) {
		    fprintf(stderr, "%s: %s is not a device\n", pname, value);
		    exit(1);
		}
		dev = st.st_rdev & 0xFFF0; /* get "parent" device */
		for (i=0; i<256; i++)
		    if (dev == bios_dev_map[i])
			break;
		if (i==256) {	       /* not found */
		    fprintf(stderr, "%s: \"%s\" (on %04X) not mapped to a"
			    " BIOS drive\n", pname, value, dev);
		    exit(1);
		}
		bios = i;
		if (MAJOR(st.st_rdev) == FLOPPY_MAJOR) {
		    cyl = head = 0;
		    sector = 1;
		} else {
		    if ( (fd = open (value, O_RDONLY)) < 0) {
			fprintf(stderr, "%s: open \"%s\": %s\n", pname, value,
				strerror(errno));
			exit(1);
		    }
		    if (ioctl (fd, HDIO_GETGEO, &hdprm) < 0) {
			fprintf(stderr, "%s: ioctl HDIO_GETGEO \"%s\": %s\n",
				pname, value, strerror(errno));
			exit(1);
		    }
		    close (fd);
		    offset = hdprm.start;
		    sector = (offset % sectors[bios]) + 1;
		    offset /= sectors[bios];
		    head = offset % heads[bios];
		    cyl = offset / heads[bios];
		}
		cx = ((cyl&0xFF) << 8) | ((cyl&0x300) >> 2) | sector;
		dx = ((head&0xFF) << 8) | (bios&0xFF);
		if (!strcasecmp(token, "DEVICE")) {
		    opts[options].u.b.bs_cx = cx;
		    opts[options].u.b.bs_dx = dx;
		} else {
		    opts[options].u.b.mbr_cx = cx;
		    opts[options].u.b.mbr_dx = dx;
		}
	    } else if (!strcasecmp(token, "ENTRY")) {
		int i = atoi(value);
		if (strlen(value) != strspn(value, "0123456789") ||
		    i<1 || i>4) {
		    fprintf(stderr, "%s: boot option \"%s\": argument to"
			    " \"entry\" should be a number, 1 to 4\n", pname,
			    opts[options].name);
		    exit(1);
		}
		opts[options].u.b.si = 0x7AE + i * 0x10;
	    } else if (!strcasecmp(token, "INSERT")) {
		unsigned short *sp = opts[options].u.b.scans;
		do {
		    if (!*value) {
			fprintf(stderr, "%s: badly formatted \"insert\" line "
				"in boot option \"%s\"\n", pname,
				opts[options].name);
			exit(1);
		    }
		    if (value[1]==',' || value[1]=='\0') {
			*sp++ = (*value ? *value : 0x300);
			value++;
		    } else if (isdigit(*value)) {
			int i = 0;
			while (isdigit(*value))
			    i = i*10 + (*value++ - '0');
			*sp++ = (i ? i : 0x300);
		    } else { /* parse as function key */
			struct fk *f;
			int len = strcspn(value, ",");
			*sp = 0;
			for (f=fknames;
			     f<fknames+sizeof(fknames)/sizeof(*fknames);
			     f++)
			    if (!strncasecmp(f->name, value, len)) {
				*sp = f->code << 8;
				break;
			    }
			if (!*sp) {
			    fprintf(stderr, "%s: string \"%s\" does not "
				    "correspond to a known function key\n",
				    pname, value);
			    exit(1);
			}
			sp++;
			value += len;
		    }
		    if (*value && *value != ',') {
			fprintf(stderr, "%s: badly formatted \"insert\" line "
				"in boot option \"%s\"\n", pname,
				opts[options].name);
			exit(1);
		    }
		    if (*value)
			value++;
		} while (*value);
		*sp = 0;
	    } else {
		fprintf(stderr, "%s: unrecognised token \"%s\" in boot"
			" option \"%s\"\n", pname, token,
			opts[options].name);
		exit(1);
	    }
	    break;
	  default:
	    fprintf(stderr, "%s: boot option \"%s\" has no type"
		    " specified before \"%s\" token\n", pname,
		    opts[options].name, token);
	    exit(1);
	}
    }
}

/* Round off a section (typically, check everything's there) */
static void end_sect(enum SECT sect) {
    int err = FALSE;
    if (sect == DBOOT) {
	if (mbr == -1) {
	    fprintf(stderr, "%s: \"mbr\" line not in [DBOOT] section\n",
		    pname);
	    err = TRUE;
	}
	if (!*loader) {
	    fprintf(stderr, "%s: \"loader\" line not in [DBOOT] section\n",
		    pname);
	    err = TRUE;
	}
	if (!*bdev_dir) {
	    fprintf(stderr, "%s: \"mounted\" line not in [DBOOT] section\n",
		    pname);
	    err = TRUE;
	}
	if (!*message) {
	    fprintf(stderr, "%s: \"message\" line not in [DBOOT] section\n",
		    pname);
	    err = TRUE;
	}
	if (!*bsector_name) {
	    fprintf(stderr, "%s: \"bootsector\" line not in [DBOOT] section\n",
		    pname);
	    err = TRUE;
	}
	if (bdev_bios == -1) {
	    fprintf(stderr, "%s: \"partition\" line not in [DBOOT] section\n",
		    pname);
	    err = TRUE;
	}
    } else if (sect != BIOS) {
	if (opts[options].type == NOTYPE) {
	    fprintf(stderr, "%s: \"mbr\" line not in boot option \"%s\"\n",
		    pname, opts[options].name);
	    err = TRUE;
	}
	if (!opts[options].keys[0]) {
	    fprintf(stderr, "%s: \"keys\" line not in boot option \"%s\"\n",
		    pname, opts[options].name);
	    err = TRUE;
	}
	switch (opts[options].type) {
	  case KERN:
	    if (!opts[options].u.k.dosname[0]) {
		fprintf(stderr,
			"%s: \"filename\" line not in boot option \"%s\"\n",
			pname, opts[options].name);
		err = TRUE;
	    }
	    break;
	  case BFILE:
	    if (!opts[options].u.bf.dosname[0]) {
		fprintf(stderr,
			"%s: \"filename\" line not in boot option \"%s\"\n",
			pname, opts[options].name);
		err = TRUE;
	    }
	    break;
	  case BSECT:
	    if (opts[options].u.b.bs_cx == -1) {
		fprintf(stderr,
			"%s: \"device\" line not in boot option \"%s\"\n",
			pname, opts[options].name);
		err = TRUE;
	    }
	    if (opts[options].u.b.mbr_cx!=-1 && opts[options].u.b.si==-1) {
		fprintf(stderr, "%s: \"entry\" specified without \"table\" "
			"in boot option \"%s\"\n",
			pname, opts[options].name);
		err = TRUE;
	    }
	    if (opts[options].u.b.mbr_cx==-1 && opts[options].u.b.si!=-1) {
		fprintf(stderr, "%s: \"entry\" specified without \"table\" "
			"in boot option \"%s\"\n",
			pname, opts[options].name);
		err = TRUE;
	    }
	    break;
	}
	options++;
    }
    if (err)
	exit(1);
}

/* Parse a DOS file name into its internal representation */
static void dos_parse_fname (char *result, char *name) {
    char *p = result, *q = name;
    int i;

    memset(result, 0, 14);
    i = 8;
    while (i>0 && *q && *q!='.')
	*p++ = toupper(*q), i--, q++;
    if (i==0 && *q && *q!='.') {
	fprintf(stderr, "%s: file name \"%s\" exceeds DOS length limit\n",
		pname, name);
	exit(1);
    }
    if (*q) {
	q++;
	while (i--) *p++ = ' ';
	i = 3;
	while (i>0 && *q)
	    *p++ = toupper(*q), i--, q++;
	if (i==0 && *q) {
	    fprintf(stderr, "%s: file name \"%s\" exceeds DOS length limit\n",
		    pname, name);
	    exit(1);
	}
    } else
	i += 3;
    while (i--) *p++ = ' ';
}

/* Do the write: construct and write the loader and boot sector */
static void do_write(int test_mode) {
    static char buffer[4096];
    int fd, i, j;
    char *p, *q, *r, *s;

    /* Construct the loader */
    memcpy (buffer, ld_data, ld_data_len);
    p = buffer + ld_data_len;
    memcpy(p, &bdev_start, 4), p+=4;   /* partition offset */
    memcpy(p, &sectors[bdev_bios], 2), p+=2; /* # of sectors */
    memcpy(p, &heads[bdev_bios], 2), p+=2; /* # of heads */
    memcpy(p, &bdev_bios, 1), p+=1;    /* the boot device BIOS number */
    memcpy(p, &nodefault, 1), p+=1;    /* for no default boot option */
    q = p + 2 + CONF_LEN*options + 2;  /* name ptr + config table */
    i = q - buffer; memcpy(p, &i, 2); p+=2; /* name ptr */
    r = message; while (*q++ = *r++);  /* name itself */
    for (j=0; j<options; j++) {
	i = q - buffer; memcpy(p, &i, 2); p+=2; /* key ptr */
	r = opts[j].keys; while (*q++ = *r++); /* key list */
	memcpy(p, &opts[j].type, 2); p+=2; /* option type */
	i = q - buffer; memcpy(p, &i, 2); p+=2; /* param ptr */
	switch (opts[j].type) {	       /* param blk */
	  case KERN:
	    s = q, q += 4;	       /* kernel blk size 4 */
	    i = q - buffer; memcpy(s, &i, 2); s+=2; /* name ptr */
	    memcpy(q, opts[j].u.k.dosname, 11); q+=11; /* name itself */
	    i = q - buffer; memcpy(s, &i, 2); s+=2; /* cmdline ptr */
	    r = opts[j].u.k.cmdline; while (*q++ = *r++); /* cmdline itself */
	    break;
	  case BFILE:
	    s = q, q += 4;	       /* bootfile blk size 2 */
	    i = q - buffer; memcpy(s, &i, 2); s+=2; /* name ptr */
	    memcpy(q, opts[j].u.bf.dosname, 11); q+=11; /* name itself */
	    if (opts[j].u.bf.scans[0]) {
		unsigned short *sp = opts[j].u.bf.scans;
		i = q - buffer; memcpy(s, &i, 2); s+=2;  /* scancodes */
		do {
		    *q++ = (*sp) & 0xFF;
		    *q++ = (*sp) >> 8;
		} while (*sp++);
	    } else {
		i = 0; memcpy(s, &i, 2); s+=2; /* no scancodes */
	    }
	    break;
	  case BSECT:
	    s = q, q += 12;	       /* bootsector blk size 12 */
	    memcpy (s, &opts[j].u.b.mbr_cx, 2); s+=2;
	    memcpy (s, &opts[j].u.b.mbr_dx, 2); s+=2;
	    memcpy (s, &opts[j].u.b.si, 2); s+=2;
	    memcpy (s, &opts[j].u.b.bs_cx, 2); s+=2;
	    memcpy (s, &opts[j].u.b.bs_dx, 2); s+=2;
	    if (opts[j].u.b.scans[0]) {
		unsigned short *sp = opts[j].u.b.scans;
		i = q - buffer; memcpy(s, &i, 2); s+=2;  /* scancodes */
		do {
		    *q++ = (*sp) & 0xFF;
		    *q++ = (*sp) >> 8;
		} while (*sp++);
	    } else {
		i = 0; memcpy(s, &i, 2); s+=2; /* no scancodes */
	    }
	    break;
	}
	i = q - buffer; memcpy(p, &i, 2); p+=2;/* title ptr */
	r = opts[j].name; while (*q++ = *r++);/* title */
    }

    /* Write the loader */
    if (!test_mode) {
	strcat(bdev_dir, "/");
	strcat(bdev_dir, loader);
    } else {
	strcpy (bdev_dir, "test.loader");
    }
    printf("%sWriting %s...", verbose ? "\n" : "", bdev_dir);
    fflush(stdout);
    if ( (fd = creat(bdev_dir, 0666)) < 0) {
	fprintf(stderr, "%s: creat %s: %s\n", pname, bdev_dir,
		strerror(errno));
	exit(1);
    }
    write(fd, buffer, q-buffer);
    fchmod (fd, 0444);
    close(fd);
    printf("done\n");

    /* Construct the boot sector */
    memset (buffer, 0, 512);
    memcpy (buffer, bs_data, bs_data_len);
    if (mbr) {
	fd = open(bsector_name, O_RDONLY);
	lseek (fd, 0x1BE, SEEK_SET);   /* point to partition table */
	read (fd, buffer+0x1BE, 0x40);
	close (fd);
    }
    p = buffer+bs_data_offset;	       /* the boot device data */
    memcpy(p, &bdev_bios, 1), p+=1;    /* the boot device BIOS number */
    memcpy(p, &sectors[bdev_bios], 2), p+=2;    /* # of sectors */
    memcpy(p, &heads[bdev_bios], 2), p+=2;    /* # of heads */
    memcpy(p, &bdev_start, 4), p+=4;   /* partition offset */
    memcpy(p, &dosloader, 11), p+=11;  /* DOS file name of loader */
    buffer[510] = 0x55;
    buffer[511] = 0xAA;
    
    /* Write the boot sector */
    if (test_mode) {
	printf("Writing test.bootsect...", bdev_dir);
	fflush(stdout);
	if ( (fd = creat("test.bootsect", 0666)) < 0) {
	    fprintf(stderr, "%s: creat test.bootsect: %s\n", pname,
		    strerror(errno));
	    exit(1);
	}
	write(fd, buffer, 512);
	close(fd);
	printf("done\n");
    } else {
	if ( (fd = open(bsector_name, O_RDWR)) < 0) {
	    fprintf(stderr, "%s: open %s: %s\n", pname, bsector_name,
		    strerror(errno));
	    exit(1);
	}
	lseek (fd, 0, SEEK_SET);
	read (fd, buffer+512, 512);
	if (memcmp(buffer, buffer+512, 512)) {
	    printf("Writing boot sector to %s...", bsector_name);
	    fflush (stdout);
	    lseek (fd, 0, SEEK_SET);
	    write (fd, buffer, 512);
	    printf("done\n");
	} else {
	    printf("No change needed to boot sector of %s\n", bsector_name);
	}
	close (fd);
    }
}

/* Read a line from a file, trim newline/whitespace off
 * the ends, and return pointer to the start in p, or NULL.
 * If a blank line (except for whitespace) is read, keep reading
 * more, and don't return the blank one(s). */
static char *getline (FILE *fp) {
    static char buf[512];
    char *p;
    int l;

    do {
	if (!fgets (buf, sizeof(buf), fp))
	    return NULL;

	if (buf[strlen(buf)-1]=='\n')
	    buf[strlen(buf)-1] = '\0';

	/* remove comment */
	p = buf;
	while (*p) {
	    if (*p=='"') {
		while (*p && *p!='"') p++;
	    }
	    if (*p=='#')
		*p = '\0';
	    if (*p) p++;
	}

	/* trim whitespace */
	p = buf;
	while (isspace(*p)) p++;
	l = strlen(p);
	while (l>0 && isspace(p[l-1]))
	    p[--l] = '\0';

    } while (strlen(p)==0);
    return p;
}
