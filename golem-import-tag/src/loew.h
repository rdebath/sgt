#ifndef GOLEM_LOEW_H
#define GOLEM_LOEW_H

#define MAXSTR 512		       /* should be enough */

#define PRIV_ROOT           0	       /* for users: can do anything */
#define PRIV_EXPORT        10 	       /* for users: can use the EXPORT cmd */
#define PRIV_STD           50	       /* for users: can do normal things */
#define PRIV_LOOP_CONTROL  98	       /* for funcs: can infinite-loop */
#define PRIV_UNRESTRICTED  99	       /* for funcs: unrestricted */
#define PRIV_PITS         100	       /* for users: can do @rse-all */

#ifndef FIXME
#define gmalloc(x) ({void *y = malloc((x)); fprintf(stderr, "Malloced: %ld", y);y;})
#undef gmalloc
#define gmalloc malloc
#define gfree(x) ( (x) ? free(x) : (void)0 )
#define gstrdup(x) ({char *y = gmalloc(strlen(x)+1); strcpy(y,x); y;})
#endif

typedef struct Thread Thread;
typedef struct Stack Stack;
typedef struct Exec Exec;
typedef struct Defn Defn;
typedef union Data Data;
typedef struct Security Security;

typedef enum _enum_Data_type {
  DATA_INT,
  DATA_STR,
  DATA_FILE
} Data_type;

struct Exec {
    Exec *next, *jump;
    enum { EXEC_OBJECT, EXEC_ASSIGN, EXEC_CONSTANT } type;
    Defn *defn;
    Data *data;
};

struct Defn {
    Defn *next, *nextthis;
    char *name;
    Security *user;
    int privlevel, wprivlevel;
    enum { DEFN_PRIM, DEFN_FUNC, DEFN_DATA } type;
    char *(*prim) (Thread *);
    Exec *func;
    Data *data;
};

union Data {
    Data_type type;
    struct {
	int type;
	char data[MAXSTR];
	int length;
    } string;
    struct {
	int type;
	int value;
    } integer;
    struct {
	int type;
	int fd;
    } file;
};

struct Stack {
    Data **stack;
    unsigned int sp;
    unsigned int stacklimit;
};

struct Thread {
    int suspended;		       /* can be set by the `suspend' func */
    Data **stack;
    int sp, stacklimit;
    Exec **estack;
    int esp, estacklimit;
    int cycles, cyclelimit;
    Security *user;
};

struct Security {
    char *username;
    int privlevel;		       /* generic privilege level */
    Defn **permissions;		       /* specific extra permissions */
    int permsize, permnum;
};

extern Security *loew_newusr (char *name, int privlevel);
extern char *loew_define (char *name, int type, void *data,
			  Security *user, int privlevel);
extern Defn *loew_finddefn (char *name, Security *user);
extern void loew_initdefs(void);
extern Data *loew_dupdata(Data *d);
extern Exec *loew_compile (char *program, Security *user, char **err);
extern Exec *loew_onefunc (char *function, Security *user, char **err);
void loew_freeexec (Exec *e);
extern Thread *loew_initthread (Security *user, Exec *start);
extern char *loew_step (Thread *t);
extern void loew_freethread (Thread *t);

#endif
