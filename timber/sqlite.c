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
#ifndef NOSYNC
    char *err;
    assert(db != NULL);
    assert(!transaction_open);
    sqlite_exec(db, "BEGIN;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    transaction_open = TRUE;
#endif
}

void db_rollback(void)
{
#ifndef NOSYNC
    char *err;
    assert(db != NULL);
    assert(transaction_open);
    sqlite_exec(db, "ROLLBACK;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    transaction_open = FALSE;
#endif
}

void db_commit(void)
{
#ifndef NOSYNC
    char *err;
    assert(db != NULL);
    assert(transaction_open);
    sqlite_exec(db, "COMMIT;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
    transaction_open = FALSE;
#endif
}

void db_close(void)
{
    if (transaction_open)
	db_rollback();
    if (db) {
#ifdef NOSYNC
	char *err;
	sqlite_exec(db, "COMMIT;", sqlite_null_callback, NULL, &err);
#endif
	sqlite_close(db);
    }
}

void db_open(void)
{
    struct stat sb;
    char *err;

    if (db)
	return;			       /* already open! */

    dbpath = smalloc(5 + strlen(dirpath));
    sprintf(dbpath, "%s/db", dirpath);

    if (stat(dbpath, &sb) < 0 && errno == ENOENT)
	fatal(err_perror, dbpath, "stat");

    db = sqlite_open(dbpath, 0666, &err);
    if (!db)
	fatal(err_noopendb, dbpath, err);

#ifdef NOSYNC
    sqlite_exec(db, "BEGIN;", sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
#endif
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
	const char *def = cfg_default_str(key);
	val = smalloc(1+strlen(def));
	strcpy(val, def);
	return val;
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
    char buf[40], *ego;
    sprintf(buf, "E%.0f", count);
    ego = smalloc(1+strlen(buf));
    strcpy(ego, buf);
    return ego;
}

/* ----------------------------------------------------------------------
 * This code is a client of rfc822.c; it parses a message, collects
 * information about it to file in the database, and files it.
 */
struct db_info_ctx {
    const char *ego;
    int addr_index;
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
			   info->u.mid.index,
			   info->u.mid.mid);
	if (err) fatal(err_dberror, err);
	break;
      case PARSE_SUBJECT:
	sqlite_exec_printf(db, "INSERT INTO subjects VALUES ("
			   "'%q', '%q' );",
			   sqlite_null_callback, NULL, &err,
			   ctx->ego,
			   info->u.string);
	if (err) fatal(err_dberror, err);
	break;
      case PARSE_DATE:
	fmt_date(info->u.date, datebuf);
	sqlite_exec_printf(db, "INSERT INTO dates VALUES ("
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
			   info->u.md.cd_inline ? "inline" : "attachment",
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
    ctx.addr_index = 0;

    parse_message(message, msglen, null_output_fn, NULL, db_info_fn, &ctx);

    sqlite_exec_printf(db, "INSERT INTO messages VALUES ("
		       "'%q', '%q' );",
		       sqlite_null_callback, NULL, &err,
		       ego, location);
    if (err) fatal(err_dberror, err);

    db_commit();

    if (myego)
	sfree(myego);
}
