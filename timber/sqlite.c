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
#include "charset.h"

static char *dbpath;

static int sqlite_null_callback(void *vp, int i, char **cp1, char **cp2)
{
    return 0;
}

void db_init(void)
{
    sqlite *db;
    char *err;

    dbpath = snewn(5 + strlen(dirpath), char);
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
		"  value TEXT,"
		"  PRIMARY KEY (key));",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sqlite_exec(db,
		"CREATE TABLE addresses ("
		"  ego TEXT,"
		"  header TEXT,"
		"  idx INTEGER,"
		"  displayname TEXT,"
		"  address TEXT,"
		"  PRIMARY KEY (ego, header, idx));",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    sqlite_exec(db,
		"CREATE INDEX idx_addresses_displayname"
		"  ON addresses (displayname, address);",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    sqlite_exec(db,
		"CREATE INDEX idx_addresses_address"
		"  ON addresses (address, displayname);",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sqlite_exec(db,
		"CREATE TABLE messageids ("
		"  ego TEXT,"
		"  header TEXT,"
		"  idx INTEGER,"
		"  messageid TEXT,"
		"  PRIMARY KEY (ego, header, idx));",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    sqlite_exec(db,
		"CREATE INDEX idx_messageids_messageid"
		"  ON messageids (messageid, header);",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sqlite_exec(db,
		"CREATE TABLE subjects ("
		"  ego TEXT,"
		"  subject TEXT,"
		"  PRIMARY KEY (ego));",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sqlite_exec(db,
		"CREATE TABLE dates ("
		"  ego TEXT,"
		"  date TEXT,"
		"  PRIMARY KEY (ego));",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sqlite_exec(db,
		"CREATE TABLE mimeparts ("
		"  ego TEXT,"
		"  idx INTEGER,"
		"  major TEXT,"
		"  minor TEXT,"
		"  charset TEXT,"
		"  encoding TEXT,"
		"  disposition TEXT,"
		"  filename TEXT,"
		"  description TEXT,"
		"  offset INTEGER,"
		"  length INTEGER,"
		"  PRIMARY KEY (ego, idx));",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sqlite_exec(db,
		"CREATE TABLE messages ("
		"  ego TEXT,"
		"  location TEXT,"
		"  PRIMARY KEY (ego));",
		sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);

    sqlite_close(db);
}

static sqlite *db = NULL;
int transaction_open = FALSE;

void db_begin(void)
{
    if (!nosync) {
	char *err;
	assert(db != NULL);
	assert(!transaction_open);
	sqlite_exec(db, "BEGIN;", sqlite_null_callback, NULL, &err);
	if (err) fatal(err_dberror, err);
	transaction_open = TRUE;
    }
}

void db_rollback(void)
{
    if (!nosync) {
	char *err;
	assert(db != NULL);
	assert(transaction_open);
	sqlite_exec(db, "ROLLBACK;", sqlite_null_callback, NULL, &err);
	if (err) fatal(err_dberror, err);
	transaction_open = FALSE;
    }
}

void db_commit(void)
{
    if (!nosync) {
	char *err;
	assert(db != NULL);
	assert(transaction_open);
	sqlite_exec(db, "COMMIT;", sqlite_null_callback, NULL, &err);
	if (err) fatal(err_dberror, err);
	transaction_open = FALSE;
    }
}

void db_close(void)
{
    if (transaction_open)
	db_rollback();
    if (db) {
	if (nosync) {
	    char *err;
	    sqlite_exec(db, "COMMIT;", sqlite_null_callback, NULL, &err);
	}
	sqlite_close(db);
    }
}

void db_open(void)
{
    struct stat sb;
    char *err;

    if (db)
	return;			       /* already open! */

    dbpath = snewn(5 + strlen(dirpath), char);
    sprintf(dbpath, "%s/db", dirpath);

    if (stat(dbpath, &sb) < 0 && errno == ENOENT)
	fatal(err_perror, dbpath, "stat");

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, dbpath, err);

    if (nosync) {
	sqlite_exec(db, "BEGIN;", sqlite_null_callback, NULL, &err);
	if (err) fatal(err_dberror, err);
    }
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
    if (rows > 0) {
	assert(cols == 1);
	ret = dupstr(table[1]);
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
	const char *def = cfg_default_str(key);
	return dupstr(def);
    } else
	return val;
}

void cfg_set_str(char *key, char *str)
{
    char *err;

    db_open();

    sqlite_exec_printf(db, "INSERT OR REPLACE INTO config VALUES ('%q','%q');",
		       sqlite_null_callback, NULL, &err,
		       key, str);
    if (err) fatal(err_dberror, err);
}

void cfg_set_int(char *key, int val)
{
    char str[64];

    sprintf(str, "%d", val);
    cfg_set_str(key, str);
}

/* ----------------------------------------------------------------------
 * Manufacture a message ego string from a number.
 */
char *invent_ego(double count)
{
    /*
     * FIXME: I would like to make these strings more cryptic and
     * more variable; I'd like the user to have a very low chance
     * of seeing two almost-identical-looking egos. Some sort of
     * hash or cipher might be the answer, although cryptographic
     * security is not required, only mild visual scrambling.
     */
    char buf[40];
    sprintf(buf, "E%.0f", count);
    return dupstr(buf);
}

/* ----------------------------------------------------------------------
 * This code is a client of rfc822.c; it parses a message, collects
 * information about it to file in the database, and files it.
 */
struct db_info_ctx {
    const char *ego;
    int addr_index;
    int mid_index;
    int mime_index;
};
void db_info_fn(void *vctx, struct message_parse_info *info)
{
    struct db_info_ctx *ctx = (struct db_info_ctx *)vctx;
    char datebuf[DATEBUF_MAX];
    char *err;

    switch (info->type) {
      case PARSE_ADDRESS:
	sqlite_exec_printf(db, "INSERT INTO addresses VALUES ("
			   "'%q', '%q', %d, '%q', '%q' );",
			   sqlite_null_callback, NULL, &err,
			   ctx->ego,
			   header_name(info->header),
			   ctx->addr_index++,
			   info->u.addr.display_name,
			   info->u.addr.address);
	if (err) fatal(err_dberror, err);
	break;
      case PARSE_MESSAGE_ID:
	sqlite_exec_printf(db, "INSERT INTO messageids VALUES ("
			   "'%q', '%q', %d, '%q' );",
			   sqlite_null_callback, NULL, &err,
			   ctx->ego,
			   header_name(info->header),
			   ctx->mid_index++,
			   info->u.string);
	if (err) fatal(err_dberror, err);
	break;
      case PARSE_SUBJECT:
	sqlite_exec_printf(db, "INSERT OR REPLACE INTO subjects VALUES ("
			   "'%q', '%q' );",
			   sqlite_null_callback, NULL, &err,
			   ctx->ego,
			   info->u.string);
	if (err) fatal(err_dberror, err);
	break;
      case PARSE_DATE:
	fmt_date(info->u.date, datebuf);
	sqlite_exec_printf(db, "INSERT OR REPLACE INTO dates VALUES ("
			   "'%q', '%q' );",
			   sqlite_null_callback, NULL, &err,
			   ctx->ego,
			   datebuf);
	if (err) fatal(err_dberror, err);
	break;
      case PARSE_MIME_PART:
	sqlite_exec_printf(db, "INSERT INTO mimeparts VALUES ("
			   "'%q', %d, '%q', '%q', '%q', '%q',"
			   "'%q', '%q', '%q', %d, %d);",
			   sqlite_null_callback, NULL, &err,
			   ctx->ego,
			   ctx->mime_index++,
			   info->u.md.major, info->u.md.minor,
			   charset_to_localenc(info->u.md.charset),
			   encoding_name(info->u.md.transfer_encoding),
			   disposition_name(info->u.md.disposition),
			   info->u.md.filename ? info->u.md.filename : "",
			   info->u.md.description ? info->u.md.description:"",
			   info->u.md.offset, info->u.md.length);
	if (err) fatal(err_dberror, err);
	break;
    }
}
void parse_for_db(const char *ego, const char *location,
		  const char *message, int msglen)
{
    char *err;
    char *myego = NULL;
    struct db_info_ctx ctx;

    db_open();
    db_begin();

    if (!ego) {
	char *countstr = cfg_get_str("num-messages");
	double count = atof(countstr), count2;
	sfree(countstr);
	myego = invent_ego(count);
	count2 = count + 1;
	if (count2 == count)
	    fatal(err_dbfull);
	cfg_set_int("num-messages", count2);
	ego = myego;
    }

    ctx.ego = ego;
    ctx.addr_index = ctx.mid_index = ctx.mime_index = 0;

    parse_message(message, msglen, null_output_fn, NULL, db_info_fn, &ctx,
		  CS_NONE);

    sqlite_exec_printf(db, "INSERT INTO messages VALUES ("
		       "'%q', '%q' );",
		       sqlite_null_callback, NULL, &err,
		       ego, location);
    if (err) fatal(err_dberror, err);

    db_commit();

    if (myego)
	sfree(myego);
}

char *message_location(const char *ego)
{
    char **table;
    int rows, cols;
    char *err;
    char *ret;

    db_open();

    sqlite_get_table_printf(db, "SELECT location FROM messages WHERE ego='%q'",
			    &table, &rows, &cols, &err, ego);
    if (err) fatal(err_dberror, err);
    if (rows > 0) {
	assert(cols == 1);
	ret = dupstr(table[1]);
    } else
	ret = NULL;
    sqlite_free_table(table);

    return ret;
}

struct mime_details *find_mime_parts(const char *ego, int *nparts)
{
    char **table;
    int rows, cols, i;
    char *err;
    struct mime_details *ret;

    db_open();

    sqlite_get_table_printf(db, "SELECT major, minor, charset, encoding,"
			    " disposition, filename, description, offset,"
			    " length, idx FROM mimeparts WHERE ego='%q'"
			    " ORDER BY idx",
			    &table, &rows, &cols, &err, ego);
    if (err) fatal(err_dberror, err);
    if (rows > 0) {
	assert(cols == 10);
	ret = snewn(rows, struct mime_details);
	for (i = 1; i < rows+1; i++) {
	    ret[i-1].major = dupstr(table[cols*i + 0]);
	    ret[i-1].minor = dupstr(table[cols*i + 1]);
	    ret[i-1].charset = charset_from_localenc(table[cols*i + 2]);
	    ret[i-1].transfer_encoding = encoding_val(table[cols*i + 3]);
	    ret[i-1].disposition = disposition_val(table[cols*i + 4]);
	    ret[i-1].filename = (*table[cols*i + 5] ?
			    dupstr(table[cols*i + 5]) : NULL);
	    ret[i-1].description = (*table[cols*i + 6] ?
			       dupstr(table[cols*i + 6]) : NULL);
	    ret[i-1].offset = atoi(table[cols*i + 7]);
	    ret[i-1].length = atoi(table[cols*i + 8]);
	    if (i-1 != atoi(table[cols*i + 9]))
		error(err_internal, "MIME part indices not contiguous");
	}
	*nparts = rows;
    } else
	ret = NULL;
    sqlite_free_table(table);

    return ret;
}
