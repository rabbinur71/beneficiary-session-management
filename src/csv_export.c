#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <glib/gstdio.h>
#include "database.h"
#include "csv_export.h"

#define MAX_CASES 5000

typedef struct {
    char case_id[128];
    int counts[4];
} CaseCount;

typedef struct {
    char case_id[128];
    char name[256];
    char sex[32];
    char age_group[32];
} TypeCase;

static int parse_date_to_int(const char *date)
{
    int y, m, d;

    if (date == NULL) {
        return 0;
    }

    if (sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) {
        return 0;
    }

    return y * 10000 + m * 100 + d;
}

static int age_index(const char *age_group)
{
    if (age_group == NULL) {
        return -1;
    }

    if (strcmp(age_group, "5-11") == 0) return 0;
    if (strcmp(age_group, "12-17") == 0) return 1;
    if (strcmp(age_group, "18-59") == 0) return 2;
    if (strcmp(age_group, "60+") == 0) return 3;

    return -1;
}

static int find_or_add_case(CaseCount cases[], int *case_count, const char *case_id)
{
    if (case_id == NULL || case_id[0] == '\0') {
        return -1;
    }

    for (int i = 0; i < *case_count; i++) {
        if (strcmp(cases[i].case_id, case_id) == 0) {
            return i;
        }
    }

    if (*case_count >= MAX_CASES) {
        return -1;
    }

    strncpy(cases[*case_count].case_id, case_id, sizeof(cases[*case_count].case_id) - 1);
    cases[*case_count].case_id[sizeof(cases[*case_count].case_id) - 1] = '\0';

    for (int i = 0; i < 4; i++) {
        cases[*case_count].counts[i] = 0;
    }

    (*case_count)++;
    return (*case_count) - 1;
}

static int type_case_exists(TypeCase list[], int count, const char *case_id)
{
    if (case_id == NULL || case_id[0] == '\0') {
        return 0;
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(list[i].case_id, case_id) == 0) {
            return 1;
        }
    }

    return 0;
}

static void add_type_case(
    TypeCase list[],
    int *count,
    const char *case_id,
    const char *name,
    const char *sex,
    const char *age_group
)
{
    if (*count >= MAX_CASES) {
        return;
    }

    if (type_case_exists(list, *count, case_id)) {
        return;
    }

    strncpy(list[*count].case_id, case_id ? case_id : "", 127);
    strncpy(list[*count].name, name ? name : "", 255);
    strncpy(list[*count].sex, sex ? sex : "", 31);
    strncpy(list[*count].age_group, age_group ? age_group : "", 31);

    list[*count].case_id[127] = '\0';
    list[*count].name[255] = '\0';
    list[*count].sex[31] = '\0';
    list[*count].age_group[31] = '\0';

    (*count)++;
}

/*
 * Write CSV-safe text.
 * If text contains comma, quote, or newline, it will be wrapped in quotes.
 */
static void csv_write_text(FILE *fp, const char *text)
{
    if (text == NULL) {
        return;
    }

    int need_quotes = 0;

    for (const char *p = text; *p; p++) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            need_quotes = 1;
            break;
        }
    }

    if (need_quotes) {
        fputc('"', fp);

        for (const char *p = text; *p; p++) {
            if (*p == '"') {
                fputc('"', fp);
                fputc('"', fp);
            } else {
                fputc(*p, fp);
            }
        }

        fputc('"', fp);
    } else {
        fprintf(fp, "%s", text);
    }
}

static void write_report_section(FILE *fp, const char *title, CaseCount cases[], int case_count, int totals[4], int new_cases[4], int old_cases[4])
{
    fprintf(fp, "\n");
    csv_write_text(fp, title);
    fprintf(fp, "\n");

    fprintf(fp, "Case ID,5-11 yr,12-17 yr,18-59 yr,60+ yr\n");

    for (int i = 0; i < case_count; i++) {
        csv_write_text(fp, cases[i].case_id);
        fprintf(
            fp,
            ",%d,%d,%d,%d\n",
            cases[i].counts[0],
            cases[i].counts[1],
            cases[i].counts[2],
            cases[i].counts[3]
        );
    }

    fprintf(
        fp,
        "GRAND TOTAL SESSIONS,%d,%d,%d,%d\n",
        totals[0],
        totals[1],
        totals[2],
        totals[3]
    );

    fprintf(
        fp,
        "NEW CASES,%d,%d,%d,%d\n",
        new_cases[0],
        new_cases[1],
        new_cases[2],
        new_cases[3]
    );

    fprintf(
        fp,
        "OLD CASES,%d,%d,%d,%d\n",
        old_cases[0],
        old_cases[1],
        old_cases[2],
        old_cases[3]
    );
}

static void write_type_list(FILE *fp, const char *title, TypeCase list[], int count)
{
    fprintf(fp, "\n");
    csv_write_text(fp, title);
    fprintf(fp, "\n");

    fprintf(fp, "Case ID,Name,Sex,Age Group\n");

    for (int i = 0; i < count; i++) {
        csv_write_text(fp, list[i].case_id);
        fprintf(fp, ",");

        csv_write_text(fp, list[i].name);
        fprintf(fp, ",");

        csv_write_text(fp, list[i].sex);
        fprintf(fp, ",");

        csv_write_text(fp, list[i].age_group);
        fprintf(fp, "\n");
    }
}

int export_report_csv(
    int start_int,
    int end_int,
    const char *file_path,
    char *error_message,
    int error_size
)
{
    static CaseCount male_cases[MAX_CASES];
    static CaseCount female_cases[MAX_CASES];

    static TypeCase new_list[MAX_CASES];
    static TypeCase old_list[MAX_CASES];

    int male_case_count = 0;
    int female_case_count = 0;
    int new_count = 0;
    int old_count = 0;

    int male_totals[4] = {0, 0, 0, 0};
    int female_totals[4] = {0, 0, 0, 0};

    int male_new_cases[4] = {0, 0, 0, 0};
    int female_new_cases[4] = {0, 0, 0, 0};
    int male_old_cases[4] = {0, 0, 0, 0};
    int female_old_cases[4] = {0, 0, 0, 0};

    static TypeCase counted_male_new[MAX_CASES];
    static TypeCase counted_female_new[MAX_CASES];
    int counted_male_new_count = 0;
    int counted_female_new_count = 0;
    static TypeCase counted_male_old[MAX_CASES];
    static TypeCase counted_female_old[MAX_CASES];
    int counted_male_old_count = 0;
    int counted_female_old_count = 0;

    memset(male_cases, 0, sizeof(male_cases));
    memset(female_cases, 0, sizeof(female_cases));
    memset(new_list, 0, sizeof(new_list));
    memset(old_list, 0, sizeof(old_list));
    memset(counted_male_new, 0, sizeof(counted_male_new));
    memset(counted_female_new, 0, sizeof(counted_female_new));
    memset(counted_male_old, 0, sizeof(counted_male_old));
    memset(counted_female_old, 0, sizeof(counted_female_old));

    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) {
        snprintf(error_message, error_size, "Could not open database.");
        return 0;
    }

    const char *sql =
        "SELECT date, case_id, name, age_group, sex, type, fcn "
        "FROM sessions "
        "ORDER BY date ASC, id ASC;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        snprintf(error_message, error_size, "Could not prepare report query.");
        sqlite3_close(db);
        return 0;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *date = (const char *)sqlite3_column_text(stmt, 0);
        const char *case_id = (const char *)sqlite3_column_text(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *age_group = (const char *)sqlite3_column_text(stmt, 3);
        const char *sex = (const char *)sqlite3_column_text(stmt, 4);
        const char *type = (const char *)sqlite3_column_text(stmt, 5);

        int record_date = parse_date_to_int(date);

        if (record_date < start_int || record_date > end_int) {
            continue;
        }

        int aidx = age_index(age_group);

        if (aidx < 0) {
            continue;
        }

        if (strcmp(sex, "Male") == 0) {
            int idx = find_or_add_case(male_cases, &male_case_count, case_id);

            if (idx >= 0) {
                male_cases[idx].counts[aidx]++;
                male_totals[aidx]++;
            }

            if (strcmp(type, "New Case") == 0) {
                if (!type_case_exists(counted_male_new, counted_male_new_count, case_id)) {
                    male_new_cases[aidx]++;
                    add_type_case(counted_male_new, &counted_male_new_count, case_id, name, sex, age_group);
                }
            }

            if (strcmp(type, "Old Case") == 0) {
                if (!type_case_exists(counted_male_old, counted_male_old_count, case_id)) {
                    male_old_cases[aidx]++;
                    add_type_case(counted_male_old, &counted_male_old_count, case_id, name, sex, age_group);
                }
            }
        }

        if (strcmp(sex, "Female") == 0) {
            int idx = find_or_add_case(female_cases, &female_case_count, case_id);

            if (idx >= 0) {
                female_cases[idx].counts[aidx]++;
                female_totals[aidx]++;
            }

            if (strcmp(type, "New Case") == 0) {
                if (!type_case_exists(counted_female_new, counted_female_new_count, case_id)) {
                    female_new_cases[aidx]++;
                    add_type_case(counted_female_new, &counted_female_new_count, case_id, name, sex, age_group);
                }
            }

            if (strcmp(type, "Old Case") == 0) {
                if (!type_case_exists(counted_female_old, counted_female_old_count, case_id)) {
                    female_old_cases[aidx]++;
                    add_type_case(counted_female_old, &counted_female_old_count, case_id, name, sex, age_group);
                }
            }
        }

        if (strcmp(type, "New Case") == 0) {
            add_type_case(new_list, &new_count, case_id, name, sex, age_group);
        }

        if (strcmp(type, "Old Case") == 0) {
            add_type_case(old_list, &old_count, case_id, name, sex, age_group);
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    FILE *fp = g_fopen(file_path, "w");

    if (fp == NULL) {
        snprintf(error_message, error_size, "Could not create CSV file.");
        return 0;
    }

    fprintf(fp, "Beneficiary Session Management\n");
    fprintf(fp, "Developed by Raaz © 2026\n");
    fprintf(fp, "CSV Report\n");
    fprintf(fp, "Date Range,%d to %d\n", start_int, end_int);

    write_report_section(fp, "Male Report", male_cases, male_case_count, male_totals, male_new_cases, male_old_cases);
    write_report_section(fp, "Female Report", female_cases, female_case_count, female_totals, female_new_cases, female_old_cases);

    write_type_list(fp, "New Case List", new_list, new_count);
    write_type_list(fp, "Old Case List", old_list, old_count);

    fclose(fp);

    return 1;
}
