/*
 * sqlite.c - caltrap's interface to SQLite
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
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
		"    id INTEGER,"
		"    start TEXT,"
		"    end TEXT,"
		"    length TEXT,"
		"    period TEXT,"
		"    type TEXT,"
		"    description TEXT);",
		sqlite_null_callback, NULL, &err);
    if (err)
	fatal(err_dberror, err);

    sqlite_exec(db,
		"CREATE TABLE freeids ("
		"    first INTEGER,"
		"    last INTEGER);",
		sqlite_null_callback, NULL, &err);
    if (err)
	fatal(err_dberror, err);

    sqlite_exec(db,
		"INSERT INTO freeids VALUES (0, 2147483647);",
		sqlite_null_callback, NULL, &err);
    if (err)
	fatal(err_dberror, err);

    sqlite_close(db);
}

static sqlite *db = NULL;

void db_close(void)
{
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

void db_add_entry(struct entry *ent)
{
    char *err;
    char *sdt, *edt, *len, *per;
    char **table;
    int rows, cols, id, firstid, lastid;

    db_open();

    sdt = format_datetime(ent->sd, ent->st);
    edt = format_datetime(ent->ed, ent->et);
    len = format_duration(ent->length);
    per = format_duration(ent->period);

    sqlite_exec(db, "BEGIN;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    if (ent->id < 0) {
	/*
	 * Find an ID and mark it as used.
	 */
	sqlite_get_table(db,
			 "SELECT first, last FROM freeids"
			 " ORDER BY first"
			 " LIMIT 1;",
			 &table, &rows, &cols, &err);
	if (err) fatal(err_dberror, err);
	if (rows < 1) fatal(err_dbfull, err);
	assert(cols == 2);
	ent->id = id = atoi(table[2]);
    } else {
	/*
	 * An ID has been provided; attempt to use it.
	 */
	id = ent->id;
	sqlite_get_table_printf(db,
				"SELECT first, last FROM freeids"
				" WHERE first <= %d AND last >= %d"
				" LIMIT 1;",
				&table, &rows, &cols, &err, id, id);
	if (err) fatal(err_dberror, err);
	if (rows < 1) fatal(err_dbconsist, "reused-entry-id");
	assert(cols == 2);
    }
    firstid = atoi(table[2]);
    lastid = atoi(table[3]);
    sqlite_free_table(table);
    if (firstid == lastid)
	sqlite_exec_printf(db, "DELETE FROM freeids WHERE first = %d",
			   sqlite_null_callback, NULL, &err, firstid);
    else if (firstid == id)
	sqlite_exec_printf(db,
			   "UPDATE freeids SET first = %d WHERE first = %d",
			   sqlite_null_callback, NULL, &err, id+1, firstid);
    else if (lastid == id)
	sqlite_exec_printf(db,
			   "UPDATE freeids SET last = %d WHERE first = %d",
			   sqlite_null_callback, NULL, &err, id-1, firstid);
    else {
	sqlite_exec_printf(db,
			   "UPDATE freeids SET last = %d WHERE first = %d",
			   sqlite_null_callback, NULL, &err, id-1, firstid);
	sqlite_exec_printf(db,
			   "INSERT INTO freeids VALUES ( %d, %d );",
			   sqlite_null_callback, NULL, &err, id+1, lastid);
    }
    if (err) fatal(err_dberror, err);

    sqlite_exec_printf(db,
		       "INSERT INTO entries VALUES ("
		       "%ld, '%q', '%q', '%q', '%q', '%q', '%q');",
		       sqlite_null_callback, NULL, &err,
		       id, sdt, edt, len, per, type_to_name(ent->type),
		       ent->description);
    if (err) fatal(err_dberror, err);

    sqlite_exec(db, "COMMIT;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sfree(sdt);
    sfree(edt);
    sfree(len);
    sfree(per);
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
    struct entry ent;

    assert(argc == 7);
    ent.id = atoi(argv[0]);
    if (!parse_datetime(argv[1], &ent.sd, &ent.st))
	fatal(err_dbconsist, "invalid-start-datetime");
    if (!parse_datetime(argv[2], &ent.ed, &ent.et))
	fatal(err_dbconsist, "invalid-end-datetime");
    if ((ent.length = parse_duration(argv[3])) == INVALID_DURATION)
	fatal(err_dbconsist, "invalid-length");
    if ((ent.period = parse_duration(argv[4])) == INVALID_DURATION)
	fatal(err_dbconsist, "invalid-period");
    if ((ent.type = name_to_type(argv[5])) == INVALID_TYPE)
	fatal(err_dbconsist, "invalid-type");
    ent.description = argv[6];

    s->fn(s->ctx, &ent);

    return 0;
}

void db_list_entries(Date sd, Time st, Date ed, Time et,
                     list_callback_fn_t fn, void *ctx)
{
    char *err;
    struct sqlite_list_callback_struct str;
    char *sdt, *edt;

    db_open();

    str.fn = fn;
    str.ctx = ctx;

    sdt = format_datetime(sd, st);
    edt = format_datetime(ed, et);

    sqlite_exec_printf(db,
		       "SELECT id, start, end, length, period,"
		       " type, description FROM entries"
		       " WHERE start < '%q' AND end > '%q'"
		       " ORDER BY start;",
		       sqlite_list_callback, &str, &err,
		       edt, sdt);
    if (err)
	fatal(err_dberror, err);

    sfree(sdt);
    sfree(edt);
}

void db_dump_entries(list_callback_fn_t fn, void *ctx)
{
    char *err;
    struct sqlite_list_callback_struct str;

    db_open();

    str.fn = fn;
    str.ctx = ctx;

    sqlite_exec(db, "SELECT id, start, end, length, period,"
		" type, description FROM entries;",
		sqlite_list_callback, &str, &err);
    if (err)
	fatal(err_dberror, err);
}
