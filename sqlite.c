/*
 * sqlite.c - caltrap's interface to SQLite
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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
	fatalerr_dbexists();

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatalerr_noopendb(err);

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
	fatalerr_dberror(err);

    sqlite_exec(db,
		"CREATE TABLE freeids ("
		"    first INTEGER,"
		"    last INTEGER);",
		sqlite_null_callback, NULL, &err);
    if (err)
	fatalerr_dberror(err);

    sqlite_exec(db,
		"INSERT INTO freeids VALUES (0, 2147483647);",
		sqlite_null_callback, NULL, &err);
    if (err)
	fatalerr_dberror(err);

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
    if (err) fatalerr_dberror(err);
    transaction_open = TRUE;
}

void db_rollback(void)
{
    char *err;
    assert(db != NULL);
    assert(transaction_open);
    sqlite_exec(db, "ROLLBACK;", sqlite_null_callback, NULL, &err);
    if (err) fatalerr_dberror(err);
    transaction_open = FALSE;
}

void db_commit(void)
{
    char *err;
    assert(db != NULL);
    assert(transaction_open);
    sqlite_exec(db, "COMMIT;", sqlite_null_callback, NULL, &err);
    if (err) fatalerr_dberror(err);
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
	fatalerr_nodb();

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatalerr_noopendb(err);
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

    db_begin();

    if (ent->id < 0) {
	/*
	 * Find an ID and mark it as used.
	 */
	sqlite_get_table(db,
			 "SELECT first, last FROM freeids"
			 " ORDER BY first"
			 " LIMIT 1;",
			 &table, &rows, &cols, &err);
	if (err) fatalerr_dberror(err);
	if (rows < 1) fatalerr_dbfull();
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
	if (err) fatalerr_dberror(err);
	if (rows < 1) fatalerr_dbconsist("reused-entry-id");
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
    if (err) fatalerr_dberror(err);

    sqlite_exec_printf(db,
		       "INSERT INTO entries VALUES ("
		       "%ld, '%q', '%q', '%q', '%q', '%q', '%q');",
		       sqlite_null_callback, NULL, &err,
		       id, sdt, edt, len, per, type_to_name(ent->type),
		       ent->description);
    if (err) fatalerr_dberror(err);

    db_commit();

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
	fatalerr_dbconsist("invalid-start-datetime");
    if (!parse_datetime(argv[2], &ent.ed, &ent.et))
	fatalerr_dbconsist("invalid-end-datetime");
    if ((ent.length = parse_duration(argv[3])) == INVALID_DURATION)
	fatalerr_dbconsist("invalid-length");
    if ((ent.period = parse_duration(argv[4])) == INVALID_DURATION)
	fatalerr_dbconsist("invalid-period");
    if ((ent.type = name_to_type(argv[5])) == INVALID_TYPE)
	fatalerr_dbconsist("invalid-type");
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
	fatalerr_dberror(err);

    sfree(sdt);
    sfree(edt);
}

static void db_fetch_callback(void *ctx, struct entry *ent)
{
    struct entry *ret = (struct entry *)ctx;

    *ret = *ent;		       /* structure copy */
    ret->description = smalloc(1 + strlen(ent->description));
    strcpy(ret->description, ent->description);
    assert(ret->id != -1);
}

void db_fetch(int id, struct entry *ent)
{
    char *err;
    struct sqlite_list_callback_struct str;

    db_open();

    str.fn = db_fetch_callback;
    str.ctx = ent;

    ent->id = -1;

    sqlite_exec_printf(db,
		       "SELECT id, start, end, length, period,"
		       " type, description FROM entries"
		       " WHERE id = %d;",
		       sqlite_list_callback, &str, &err, id);
    if (err)
	fatalerr_dberror(err);

    if (ent->id == -1)
	fatalerr_idnotfound(id);
}

void db_del(int id)
{
    char *err;
    char **table;
    int rows, cols, i, irows[2][2];

    db_open();

    db_begin();

    /*
     * First verify that the entry we want to delete does actually
     * exist. If not, abandon our transaction and be on our way.
     */
    sqlite_get_table_printf(db,
			    "SELECT id FROM entries WHERE id = %d;",
			    &table, &rows, &cols, &err, id);
    if (err) fatalerr_dberror(err);
    sqlite_free_table(table);
    if (rows < 1)
	fatalerr_idnotfound(id);

    /*
     * Mark the ID as free in the free IDs table. This search will
     * find any free-IDs entry which either contains the ID or is
     * right next to it.
     */
    sqlite_get_table_printf(db,
			    "SELECT first, last FROM freeids"
			    " WHERE first <= %d AND last >= %d"
			    " ORDER BY first;",
			    &table, &rows, &cols, &err, id+1, id-1);
    if (err) fatalerr_dberror(err);
    assert(rows == 0 || cols == 2);
    if (rows > 2) fatalerr_dbconsist("freeids-too-many");
    /*
     * Parse the returned rows back into integers. While we're at
     * it, check that no returned free ID range _already_ contains
     * our ID, because that's a sign of db inconsistency.
     */
    for (i = 0; i < rows; i++) {
	irows[i][0] = atoi(table[2*i+2]);
	irows[i][1] = atoi(table[2*i+3]);
	if (id >= irows[i][0] && id <= irows[i][1])
	    fatalerr_dbconsist("deleted-id-already-free");
    }

    /*
     * Now we can free the table.
     */
    sqlite_free_table(table);

    if (rows < 1) {
	/*
	 * No rows were returned. This means that the ID was in the
	 * middle of a block of used IDs, so we must add a new row
	 * exclusively for it.
	 */
	sqlite_exec_printf(db,
			   "INSERT INTO freeids VALUES ( %d, %d );",
			   sqlite_null_callback, NULL, &err, id, id);
	if (err) fatalerr_dberror(err);
    } else if (rows == 2) {
	/*
	 * Two rows were returned. This must mean that the ID was
	 * the only thing separating two free blocks; so we merge
	 * them into a single block.
	 */
	assert(irows[0][1] == id - 1);
	assert(irows[1][0] == id + 1);
	sqlite_exec_printf(db,
			   "DELETE FROM freeids WHERE first = %d;",
			   sqlite_null_callback, NULL, &err, irows[1][0]);
	if (err) fatalerr_dberror(err);
	sqlite_exec_printf(db,
			   "UPDATE freeids SET last = %d WHERE first = %d;",
			   sqlite_null_callback, NULL, &err,
			   irows[1][1], irows[0][0]);
	if (err) fatalerr_dberror(err);
    } else {
	/*
	 * One row was returned. This means the ID is either just
	 * after the end of a block or just before the start of
	 * one; so we add it into the block, after determining
	 * which.
	 */
	if (irows[0][1] == id - 1) {
	    sqlite_exec_printf(db,
			       "UPDATE freeids SET last=%d WHERE first = %d;",
			       sqlite_null_callback, NULL, &err,
			       id, irows[0][0]);
	    if (err) fatalerr_dberror(err);
	} else if (irows[0][0] == id + 1) {
	    sqlite_exec_printf(db,
			       "UPDATE freeids SET first=%d WHERE last = %d;",
			       sqlite_null_callback, NULL, &err,
			       id, irows[0][1]);
	    if (err) fatalerr_dberror(err);
	} else
	    assert(!"This should never happen");
    }

    /*
     * Whew! Compared to that load of aggro, actually deleting the
     * entry itself will be child's play.
     */
    sqlite_exec_printf(db,
		       "DELETE FROM entries WHERE id = %d;",
		       sqlite_null_callback, NULL, &err, id);
    if (err) fatalerr_dberror(err);

    db_commit();
}

void db_update(struct entry *e)
{
    char *err;
    char **table;
    int rows, cols;

    db_open();

    db_begin();

    /*
     * First verify that the entry we want to delete does actually
     * exist. If not, abandon our transaction and be on our way.
     */
    sqlite_get_table_printf(db,
			    "SELECT id FROM entries WHERE id = %d;",
			    &table, &rows, &cols, &err, e->id);
    if (err) fatalerr_dberror(err);
    sqlite_free_table(table);
    if (rows < 1)
	fatalerr_idnotfound(e->id);

    /*
     * Now do the update. Since there are lots of different pieces
     * we _might_ want to update, and since we're in a transaction
     * anyway, the simplest thing is to issue multiple SQL commands
     * rather than try to construct a single UPDATE.
     */
    if (e->sd != INVALID_DATE && e->st != INVALID_TIME) {
	char *dt = format_datetime(e->sd, e->st);
	sqlite_exec_printf(db,
			   "UPDATE entries SET start='%q' WHERE id = %d;",
			   sqlite_null_callback, NULL, &err,
			   dt, e->id);
	if (err) fatalerr_dberror(err);
	sfree(dt);
    }
    if (e->ed != INVALID_DATE && e->et != INVALID_TIME) {
	char *dt = format_datetime(e->ed, e->et);
	sqlite_exec_printf(db,
			   "UPDATE entries SET end='%q' WHERE id = %d;",
			   sqlite_null_callback, NULL, &err,
			   dt, e->id);
	if (err) fatalerr_dberror(err);
	sfree(dt);
    }
    if (e->length != INVALID_DURATION) {
	char *d = format_duration(e->length);
	sqlite_exec_printf(db,
			   "UPDATE entries SET length='%q' WHERE id = %d;",
			   sqlite_null_callback, NULL, &err,
			   d, e->id);
	if (err) fatalerr_dberror(err);
	sfree(d);
    }
    if (e->period != INVALID_DURATION) {
	char *d = format_duration(e->period);
	sqlite_exec_printf(db,
			   "UPDATE entries SET period='%q' WHERE id = %d;",
			   sqlite_null_callback, NULL, &err,
			   d, e->id);
	if (err) fatalerr_dberror(err);
	sfree(d);
    }
    if (e->type != INVALID_TYPE) {
	sqlite_exec_printf(db,
			   "UPDATE entries SET type='%q' WHERE id = %d;",
			   sqlite_null_callback, NULL, &err,
			   type_to_name(e->type), e->id);
	if (err) fatalerr_dberror(err);
    }
    if (e->description != NULL) {
	sqlite_exec_printf(db,
			   "UPDATE entries SET description='%q' WHERE id=%d;",
			   sqlite_null_callback, NULL, &err,
			   e->description, e->id);
	if (err) fatalerr_dberror(err);
    }

    db_commit();
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
	fatalerr_dberror(err);
}
