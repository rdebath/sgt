/*
 * sqlite.c - Timber's interface to SQLite.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sqlite.h"
#include "timber.h"

char *dbpath;

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

    //FIXME;

    sqlite_close(db);
}

static sqlite *db = NULL;
int transaction_open = FALSE;

void db_begin(void)
{
    char *err;
    assert(db != NULL);
    assert(!transaction_open);
    sqlite_exec(db, "BEGIN;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    transaction_open = TRUE;
}

void db_rollback(void)
{
    char *err;
    assert(db != NULL);
    assert(transaction_open);
    sqlite_exec(db, "ROLLBACK;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    transaction_open = FALSE;
}

void db_commit(void)
{
    char *err;
    assert(db != NULL);
    assert(transaction_open);
    sqlite_exec(db, "COMMIT;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    transaction_open = FALSE;
}

void db_close(void)
{
    if (transaction_open)
	db_rollback();
    if (db)
	sqlite_close(db);
}

void db_open(void)
{
    struct stat sb;
    char *err;

    if (stat(dbpath, &sb) < 0 && errno == ENOENT)
	fatal(err_nodb);

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, err);
}

