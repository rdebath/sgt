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
#include <stdarg.h>
#include <time.h>

#include "timber.h"
#include "charset.h"


static int sqlite_null_callback(void *vp, int i, char **cp1, char **cp2)
{
    return 0;
}


struct database {
    sqlite *handle;
    int transaction_open;
    const char *filename;
    const char *const *const schema;
    const int schema_len;
};

#define DATABASE(name,schema) { NULL, FALSE, name, schema, lenof(schema) }


void sql_perform_open (struct database *db,
		       int must_exist)
{
    char *dbpath;
    char *err;

    assert (NULL == db->handle);

    dbpath = snewn(5 + strlen(dirpath) + strlen(db->filename), char);
    sprintf(dbpath, "%s/%s", dirpath, db->filename);

    if (must_exist) {
	struct stat sb;
	if (stat(dbpath, &sb) < 0 && errno == ENOENT)
	    fatal(err_perror, dbpath, "stat");
    }

    db->handle = sqlite_open(dbpath, 0666, &err);
    if (!db->handle)
	fatal(err_noopendb, dbpath, err);

    sfree (dbpath);
}


void sql_perform_close (struct database *db)
{
    sqlite_close(db->handle);
    db->handle = NULL;
}


void sql_exec (struct database *db,
	       const char *sql)
{
    char *err;
    sqlite_exec (db->handle, sql, sqlite_null_callback, NULL, &err);
    if (err) fatal(err_dberror, err);
}


void sql_exec_printf (struct database *db,
		      const char *sql_fmt,
		      ...)
{
    va_list args;
    char *err;

    va_start (args, sql_fmt);
    sqlite_exec_vprintf (db->handle, sql_fmt, sqlite_null_callback, NULL, &err,
			 args);
    if (err) fatal(err_dberror, err);
    va_end (args);
}


void sql_get_table_vprintf (struct database *db,
			   const char *sql_fmt,
			   char ***table,
			   int *row_count,
			   int *column_count,
			   va_list args)
{
    char *err;

    sqlite_get_table_vprintf (db->handle, sql_fmt, table,
			      row_count, column_count, &err, args);
    if (err) fatal(err_dberror, err);
}


void sql_get_table_printf (struct database *db,
			   const char *sql_fmt,
			   char ***table,
			   int *row_count,
			   int *column_count,
			   ...)
{
    va_list args;

    va_start (args, column_count);
    sql_get_table_vprintf (db, sql_fmt, table, row_count, column_count, args);
    va_end (args);
}


char *sql_get_value_printf (struct database *b,
			    const char *sql_fmt,
			    ...)
{
    char **table;
    int rows, cols;
    va_list args;
    char *ans = NULL;

    va_start (args, sql_fmt);
    sql_get_table_vprintf (b, sql_fmt, &table, &rows, &cols, args);
    va_end (args);

    if (0 < rows) {
	assert (1 == cols);
	assert (1 == rows);
	if (table[1]) ans = dupstr(table[1]);
    }
    sqlite_free_table(table);
    return ans;
}


enum transaction_action {
    begin_transaction,
    rollback_transaction,
    commit_transaction
};

void sql_transact (struct database *db,
		   enum transaction_action act)
{
    if (!nosync) {
	static const char *sql[] = {
	    "BEGIN;",
	    "ROLLBACK;",
	    "COMMIT;"
	};
	const int opening = begin_transaction == act;

	assert (NULL != db->handle);
	assert (0 <= act && act < lenof(sql));
	assert (opening == !db->transaction_open);

	sql_exec (db, sql[act]);
	db->transaction_open = opening;
    }
}


void sql_init (struct database *db)
{
    int i;

    sql_perform_open (db, FALSE);

    /*
     * Here we set up the database schema.
     */
    for (i = 0; i < db->schema_len; ++i) {
        sql_exec (db, db->schema[i]);
    }

    sql_perform_close(db);
}


void sql_close (struct database *db)
{
    if (db->transaction_open)
	sql_transact (db, rollback_transaction);

    if (db->handle) {
	if (nosync) {
	    char *err;
	    sqlite_exec(db->handle,"COMMIT;", sqlite_null_callback, NULL,&err);
	}
	sql_perform_close(db);
    }
}


void sql_open (struct database *db)
{
    if (db->handle)
	return;			       /* already open! */

    sql_perform_open (db, TRUE);

    if (nosync) {
	sql_exec(db, "BEGIN;");
    }
}


static const char *db_schema[] = {
    "CREATE TABLE config ("
    "  key TEXT UNIQUE ON CONFLICT REPLACE,"
    "  value TEXT,"
    "  PRIMARY KEY (key));",

    "CREATE TABLE addresses ("
    "  ego TEXT,"
    "  header TEXT,"
    "  idx INTEGER,"
    "  displayname TEXT,"
    "  address TEXT,"
    "  PRIMARY KEY (ego, header, idx));",
    "CREATE INDEX idx_addresses_displayname"
    "  ON addresses (displayname, address);",
    "CREATE INDEX idx_addresses_address"
    "  ON addresses (address, displayname);",

    "CREATE TABLE messageids ("
    "  ego TEXT,"
    "  header TEXT,"
    "  idx INTEGER,"
    "  messageid TEXT,"
    "  PRIMARY KEY (ego, header, idx));",
    "CREATE INDEX idx_messageids_messageid"
    "  ON messageids (messageid, header);",
    
    "CREATE TABLE subjects ("
    "  ego TEXT,"
    "  subject TEXT,"
    "  PRIMARY KEY (ego));",

    "CREATE TABLE dates ("
    "  ego TEXT,"
    "  date TEXT,"
    "  PRIMARY KEY (ego));",

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

    "CREATE TABLE messages ("
    "  ego TEXT,"
    "  location TEXT,"
    "  PRIMARY KEY (ego));",
};


static const char *const ab_schema[] = {
    "CREATE TABLE contacts ("
    "  contact_id INTEGER PRIMARY KEY"
    ");",
    "INSERT INTO contacts VALUES (0);",

    "CREATE TABLE attributes ("
    "  attribute_id INTEGER PRIMARY KEY,"
    "  contact_id INTEGER,"
    "  type VARCHAR NOT NULL"
    ");",

    "CREATE TABLE attr_values ("
    "  attribute_id INTEGER,"
    "  until DATETIME,"  /* exclusive */
    "  value VARCHAR NOT NULL"
    ");"
};


static struct database db[1] = { DATABASE ("db", db_schema) };
static struct database ab[1] = { DATABASE ("ab", ab_schema) };


void sql_init_all(void)
{
    sql_init(db);
    sql_init(ab);
}

void sql_close_all(void)
{
    sql_close(db);
    sql_close(ab);
}



/*
 *  Message index database.
 */

static void db_begin(void)
{
    sql_transact (db, begin_transaction);
}

static void db_commit(void)
{
    sql_transact (db, commit_transaction);
}

static void db_open(void)
{
    sql_open(db);
}

static char *cfg_get_internal(char *key)
{
    char **table;
    int rows, cols;
    char *ret;

    db_open();

    sql_get_table_printf(db, "SELECT value FROM config WHERE key = '%q'",
			 &table, &rows, &cols, key);
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
    db_open();

    sql_exec_printf(db, "INSERT OR REPLACE INTO config VALUES ('%q','%q');",
		    key, str);
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

    switch (info->type) {
      case PARSE_ADDRESS:
	sql_exec_printf(db, "INSERT INTO addresses VALUES ("
			"'%q', '%q', %d, '%q', '%q' );",
			ctx->ego,
			header_name(info->header),
			ctx->addr_index++,
			info->u.addr.display_name,
			info->u.addr.address);
	break;
      case PARSE_MESSAGE_ID:
	sql_exec_printf(db, "INSERT INTO messageids VALUES ("
			"'%q', '%q', %d, '%q' );",
			ctx->ego,
			header_name(info->header),
			ctx->mid_index++,
			info->u.string);
	break;
      case PARSE_SUBJECT:
	sql_exec_printf(db,
			"INSERT OR REPLACE INTO subjects VALUES ("
			"'%q', '%q' );",
			ctx->ego,
			info->u.string);
	break;
      case PARSE_DATE:
	fmt_date(info->u.date, datebuf);
	sql_exec_printf(db, "INSERT OR REPLACE INTO dates VALUES ("
			"'%q', '%q' );",
			ctx->ego,
			datebuf);
	break;
      case PARSE_MIME_PART:
	sql_exec_printf(db, "INSERT INTO mimeparts VALUES ("
			"'%q', %d, '%q', '%q', '%q', '%q',"
			"'%q', '%q', '%q', %d, %d);",
			ctx->ego,
			ctx->mime_index++,
			info->u.md.major, info->u.md.minor,
			charset_to_localenc(info->u.md.charset),
			encoding_name(info->u.md.transfer_encoding),
			disposition_name(info->u.md.disposition),
			info->u.md.filename ? info->u.md.filename : "",
			info->u.md.description ? info->u.md.description:"",
			info->u.md.offset, info->u.md.length);
	break;
    }
}
void parse_for_db(const char *ego, const char *location,
		  const char *message, int msglen)
{
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

    sql_exec_printf(db, "INSERT INTO messages VALUES ("
		    "'%q', '%q' );",
		    ego, location);

    db_commit();

    if (myego)
	sfree(myego);
}

char *message_location(const char *ego)
{
    char **table;
    int rows, cols;
    char *ret;

    db_open();

    sql_get_table_printf(db, "SELECT location FROM messages WHERE ego='%q'",
			 &table, &rows, &cols, ego);
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
    struct mime_details *ret;

    db_open();

    sql_get_table_printf(db,
			 "SELECT major, minor, charset, encoding,"
			 " disposition, filename, description, offset,"
			 " length, idx FROM mimeparts WHERE ego='%q'"
			 " ORDER BY idx",
			 &table, &rows, &cols, ego);
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



/*
 *  Address book database.
 */

static char *ab_get_attr (int contact_id,
			  const char *attr_type)
{
    assert (attr_type);

    sql_open(ab);
    return sql_get_value_printf (ab,
			  "SELECT value FROM attributes, attr_values "
			  "WHERE contact_id = %d AND type = '%q' AND "
			  "attributes.attribute_id = attr_values.attribute_id "
			  "AND attr_values.until IS NULL",
			  contact_id, attr_type);
}


static void ab_set_attr (int contact_id,
			 const char *attr_type,
			 const char *new_value)
{
    char *attribute_id;
    char now[DATEBUF_MAX];

    assert (attr_type);
    sql_open(ab);
    sql_transact (ab, begin_transaction);

    fmt_date (time(NULL), now);
    attribute_id =
	sql_get_value_printf (ab,
			      "SELECT attribute_id FROM attributes "
			      "WHERE contact_id = %i AND type = '%q'",
			      contact_id, attr_type);

    if (attribute_id) {
	sql_exec_printf (ab,
			 "UPDATE attr_values SET until = '%q'"
			 "WHERE attribute_id = '%q' AND until IS NULL",
			 now, attribute_id);
    }
    if (new_value) {
	if (!attribute_id) {
	    sql_exec_printf (ab,
			     "INSERT INTO attributes (contact_id, type) "
			     "VALUES (%i, '%q')",
			     contact_id, attr_type);
	    attribute_id = snewn (20, char);
	    sprintf (attribute_id, "%d", sqlite_last_insert_rowid(ab->handle));
	}
	sql_exec_printf (ab,
			 "INSERT INTO attr_values (attribute_id, value) "
			 "VALUES ('%q', '%q')",
			 attribute_id, new_value);
    }
    sql_transact (ab, commit_transaction);
    sfree(attribute_id);
}


void ab_display_attr (const char *contact_id,
		      const char *attr_type)
{
    const int cid = atoi(contact_id);
    char *value = ab_get_attr (cid, attr_type);
    printf (value ? "%d:%s\n" : "%d\n", cid, value);
    sfree(value);
}

void ab_change_attr (const char *contact_id,
		     const char *attr_type,
		     const char *new_value)
{
    ab_set_attr (atoi(contact_id), attr_type, new_value);
}

