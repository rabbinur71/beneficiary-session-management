#include <sqlite3.h>
#include <stdio.h>
#include "database.h"

#ifdef _WIN32
#define DB_PATH "C:\\ProgramData\\Beneficiary Session Management\\data\\camp.db"
#else
#define DB_PATH "data/camp.db"
#endif

sqlite3 *open_database(void)
{
    sqlite3 *db = NULL;

    int rc = sqlite3_open(DB_PATH, &db);

    if (rc != SQLITE_OK) {
        printf("Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    return db;
}

int init_database(void)
{
    sqlite3 *db = open_database();
    char *err_msg = NULL;

    if (db == NULL) {
        return 0;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS sessions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "date TEXT NOT NULL,"
        "case_id TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "age_group TEXT NOT NULL,"
        "sex TEXT NOT NULL,"
        "type TEXT NOT NULL,"
        "fcn TEXT"
        ");";

    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {
        printf("SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 0;
    }

    sqlite3_close(db);
    return 1;
}

int insert_session(
    const char *date,
    const char *case_id,
    const char *name,
    const char *age_group,
    const char *sex,
    const char *type,
    const char *fcn
)
{
    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) {
        return 0;
    }

    const char *sql =
        "INSERT INTO sessions "
        "(date, case_id, name, age_group, sex, type, fcn) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        printf("Prepare insert failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, case_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, age_group, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, sex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, fcn, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return rc == SQLITE_DONE;
}

int update_session(
    int id,
    const char *date,
    const char *case_id,
    const char *name,
    const char *age_group,
    const char *sex,
    const char *type,
    const char *fcn
)
{
    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) {
        return 0;
    }

    const char *sql =
        "UPDATE sessions SET "
        "date = ?, "
        "case_id = ?, "
        "name = ?, "
        "age_group = ?, "
        "sex = ?, "
        "type = ?, "
        "fcn = ? "
        "WHERE id = ?;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        printf("Prepare update failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_text(stmt, 1, date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, case_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, age_group, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, sex, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, fcn, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 8, id);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return rc == SQLITE_DONE;
}

int delete_session(int id)
{
    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) {
        return 0;
    }

    const char *sql = "DELETE FROM sessions WHERE id = ?;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        printf("Prepare delete failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    sqlite3_bind_int(stmt, 1, id);

    rc = sqlite3_step(stmt);

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return rc == SQLITE_DONE;
}