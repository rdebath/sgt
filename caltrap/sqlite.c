/*
 * sqlite.c - caltrap's interface to SQLite
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "sqlite.h"
#include "caltrap.h"

static int sqlite_null_callback(void *vp, int i, char **cp1, char **cp2)
{
    return 0;
}

void db_init(void)
{
    sqlite *db;
    struct stat sb;
    char *err;

    if (stat(dbpath, &sb) == 0)
	fatal(err_dbexists);

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, err);

    sqlite_exec(db,
		"CREATE TABLE entries ("
		"    date INTEGER,"
		"    time INTEGER,"
		"    description TEXT);",
		sqlite_null_callback, NULL, &err);
    if (err)
	fatal(err_dberror, err);

    sqlite_close(db);
}

void db_add_entry(Date d, Time t, char *msg)
{
    sqlite *db;
    struct stat sb;
    char *err;

    if (stat(dbpath, &sb) < 0 && errno == ENOENT)
	fatal(err_nodb);

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, err);

    sqlite_exec_printf(db,
		       "INSERT INTO entries VALUES (%ld, %ld, '%q');",
		       sqlite_null_callback, NULL, &err,
		       d, t, msg);
    if (err)
	fatal(err_dberror, err);

    sqlite_close(db);
}

struct sqlite_list_callback_struct {
    list_callback_fn_t fn;
    void *ctx;
};

static int sqlite_list_callback(void *ctx, int argc,
				char **argv, char **columns)
{
    struct sqlite_list_callback_struct *s =
	(struct sqlite_list_callback_struct *)ctx;

    assert(argc == 3);
    s->fn(s->ctx, atoi(argv[0]), atoi(argv[1]), argv[2]);

    return 0;
}

void db_list_entries(Date start, Date end, list_callback_fn_t fn, void *ctx)
{
    sqlite *db;
    struct stat sb;
    char *err;
    struct sqlite_list_callback_struct str;

    if (stat(dbpath, &sb) < 0 && errno == ENOENT)
	fatal(err_nodb);

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, err);

    str.fn = fn;
    str.ctx = ctx;

    sqlite_exec_printf(db,
		       "SELECT date, time, description FROM entries"
		       " WHERE date >= %ld AND date <= %ld"
		       " ORDER BY date;",
		       sqlite_list_callback, &str, &err,
		       start, end);
    if (err)
	fatal(err_dberror, err);

    sqlite_close(db);
}
