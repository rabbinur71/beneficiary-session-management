#ifndef DATABASE_H
#define DATABASE_H

#include <sqlite3.h>

int init_database(void);

int insert_session(
    const char *date,
    const char *case_id,
    const char *name,
    const char *age_group,
    const char *sex,
    const char *type,
    const char *fcn
);

int update_session(
    int id,
    const char *date,
    const char *case_id,
    const char *name,
    const char *age_group,
    const char *sex,
    const char *type,
    const char *fcn
);

int delete_session(int id);

sqlite3 *open_database(void);

#endif