/*
 * sqlite.c - Timber's interface to SQLite.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "sqlite.h"
#include "timber.h"

static char *dbpath;

static int sqlite_null_callback(void *vp, int i, char **cp1, char **cp2)
{
    return 0;
}

void db_init(void)
{
    sqlite *db;
    char *err;

    dbpath = smalloc(5 + strlen(dirpath));
    sprintf(dbpath, "%s/db", dirpath);

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, dbpath, err);

    /*
     * Here we set up the database schema.
     */
    sqlite_exec(db,
		"CREATE TABLE config ("
		"  key TEXT UNIQUE ON CONFLICT REPLACE,"
		"  value TEXT);"
		, sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

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

    dbpath = smalloc(5 + strlen(dirpath));
    sprintf(dbpath, "%s/db", dirpath);

    if (stat(dbpath, &sb) < 0 && errno == ENOENT)
	fatal(err_perror, dbpath, "stat");

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, dbpath, err);
}

static char *cfg_get_internal(char *key)
{
    char **table;
    int rows, cols;
    char *err;
    char *ret;

    db_open();

    sqlite_get_table_printf(db, "SELECT value FROM config WHERE key = '%q'",
			    &table, &rows, &cols, &err, key);
    if (err) fatal(err_dberror, err);
    assert(cols == 1);
    if (rows > 0) {
	ret = smalloc(1+strlen(table[1]));
	strcpy(ret, table[1]);
    } else
	ret = NULL;
    sqlite_free_table(table);

    return ret;
}

int cfg_get_int(char *key)
{
    char *val = cfg_get_internal(key);
    if (!val)
	return cfg_default_int(key);
    else
	return atoi(val);
}

char *cfg_get_str(char *key)
{
    char *val = cfg_get_internal(key);
    if (!val) {
	char *def = cfg_default_str(key);
	val = smalloc(1+strlen(def));
	strcpy(val, def);
	return val;
    } else
	return val;
}
