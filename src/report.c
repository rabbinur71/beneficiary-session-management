#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "database.h"
#include "report.h"
#include "csv_export.h"

#define MAX_CASES 5000

enum {
    R_COL_CASE_ID,
    R_COL_AGE_5_11,
    R_COL_AGE_12_17,
    R_COL_AGE_18_59,
    R_COL_AGE_60,
    R_NUM_COLS
};

typedef struct {
    char case_id[128];
    int counts[4];
} CaseCount;

static GtkWidget *report_window = NULL;
static GtkWidget *range_type_combo;
static GtkWidget *date_entry;
static GtkWidget *month_entry;
static GtkWidget *start_date_entry;
static GtkWidget *end_date_entry;
static GtkWidget *range_label;

static GtkListStore *male_store;
static GtkListStore *female_store;

static GtkWidget *male_treeview;
static GtkWidget *female_treeview;

static CaseCount male_cases[MAX_CASES];
static CaseCount female_cases[MAX_CASES];

static int male_case_count = 0;
static int female_case_count = 0;

static int male_totals[4];
static int female_totals[4];

static int male_new_cases[4];
static int female_new_cases[4];
static int male_old_cases[4];
static int female_old_cases[4];

static char counted_new_male[MAX_CASES][128];
static char counted_new_female[MAX_CASES][128];
static int counted_new_male_count = 0;
static int counted_new_female_count = 0;
static char counted_old_male[MAX_CASES][128];
static char counted_old_female[MAX_CASES][128];
static int counted_old_male_count = 0;
static int counted_old_female_count = 0;

/* Last generated range, reused by New/Old buttons */
static int current_start_int = 0;
static int current_end_int = 0;
static int report_has_generated = 0;

static int parse_date_to_int(const char *date)
{
    int y, m, d;
    if (sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) return 0;
    return y * 10000 + m * 100 + d;
}

static int age_index(const char *age_group)
{
    if (strcmp(age_group, "5-11") == 0) return 0;
    if (strcmp(age_group, "12-17") == 0) return 1;
    if (strcmp(age_group, "18-59") == 0) return 2;
    if (strcmp(age_group, "60+") == 0) return 3;
    return -1;
}

static void show_message(GtkWindow *parent, const char *title, const char *message, GtkMessageType type)
{
    GtkWidget *dialog = gtk_message_dialog_new(parent, GTK_DIALOG_MODAL, type, GTK_BUTTONS_OK, "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static GtkWidget *create_label(const char *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    return label;
}

static void clear_report_data(void)
{
    male_case_count = 0;
    female_case_count = 0;
    counted_new_male_count = 0;
    counted_new_female_count = 0;
    counted_old_male_count = 0;
    counted_old_female_count = 0;

    memset(male_cases, 0, sizeof(male_cases));
    memset(female_cases, 0, sizeof(female_cases));
    memset(male_totals, 0, sizeof(male_totals));
    memset(female_totals, 0, sizeof(female_totals));
    memset(male_new_cases, 0, sizeof(male_new_cases));
    memset(female_new_cases, 0, sizeof(female_new_cases));
    memset(male_old_cases, 0, sizeof(male_old_cases));
    memset(female_old_cases, 0, sizeof(female_old_cases));

    gtk_list_store_clear(male_store);
    gtk_list_store_clear(female_store);
}

static int find_or_add_case(CaseCount cases[], int *case_count, const char *case_id)
{
    for (int i = 0; i < *case_count; i++) {
        if (strcmp(cases[i].case_id, case_id) == 0) return i;
    }

    if (*case_count >= MAX_CASES) return -1;

    strncpy(cases[*case_count].case_id, case_id, sizeof(cases[*case_count].case_id) - 1);
    cases[*case_count].case_id[sizeof(cases[*case_count].case_id) - 1] = '\0';

    for (int i = 0; i < 4; i++) cases[*case_count].counts[i] = 0;

    (*case_count)++;
    return (*case_count) - 1;
}

static int already_counted_new(char list[][128], int count, const char *case_id)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i], case_id) == 0) return 1;
    }
    return 0;
}

static void mark_new_counted(char list[][128], int *count, const char *case_id)
{
    if (*count >= MAX_CASES) return;
    strncpy(list[*count], case_id, 127);
    list[*count][127] = '\0';
    (*count)++;
}

static void add_row(GtkListStore *store, const char *case_id, int values[4])
{
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(
        store, &iter,
        R_COL_CASE_ID, case_id,
        R_COL_AGE_5_11, values[0],
        R_COL_AGE_12_17, values[1],
        R_COL_AGE_18_59, values[2],
        R_COL_AGE_60, values[3],
        -1
    );
}

static void fill_report_table(void)
{
    for (int i = 0; i < male_case_count; i++) add_row(male_store, male_cases[i].case_id, male_cases[i].counts);
    add_row(male_store, "GRAND TOTAL SESSIONS", male_totals);
    add_row(male_store, "NEW CASES", male_new_cases);
    add_row(male_store, "OLD CASES", male_old_cases);

    for (int i = 0; i < female_case_count; i++) add_row(female_store, female_cases[i].case_id, female_cases[i].counts);
    add_row(female_store, "GRAND TOTAL SESSIONS", female_totals);
    add_row(female_store, "NEW CASES", female_new_cases);
    add_row(female_store, "OLD CASES", female_old_cases);
}

static void add_report_column(GtkWidget *treeview, const char *title, int column_id)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title, renderer, "text", column_id, NULL
    );

    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 90);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
}

static GtkWidget *create_report_tree(GtkListStore **store, GtkWidget **treeview_out)
{
    *store = gtk_list_store_new(
        R_NUM_COLS,
        G_TYPE_STRING,
        G_TYPE_INT,
        G_TYPE_INT,
        G_TYPE_INT,
        G_TYPE_INT
    );

    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(*store));
    *treeview_out = treeview;

    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview), GTK_TREE_VIEW_GRID_LINES_BOTH);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);

    add_report_column(treeview, "Case ID", R_COL_CASE_ID);
    add_report_column(treeview, "5-11 yr", R_COL_AGE_5_11);
    add_report_column(treeview, "12-17 yr", R_COL_AGE_12_17);
    add_report_column(treeview, "18-59 yr", R_COL_AGE_18_59);
    add_report_column(treeview, "60+ yr", R_COL_AGE_60);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), treeview);

    return scroll;
}

static int calculate_week_range(const char *input_date, int *start_int, int *end_int, char *label_text, int label_size)
{
    int y, m, d;

    if (sscanf(input_date, "%d-%d-%d", &y, &m, &d) != 3) return 0;

    struct tm tm_date;
    memset(&tm_date, 0, sizeof(tm_date));

    tm_date.tm_year = y - 1900;
    tm_date.tm_mon = m - 1;
    tm_date.tm_mday = d;
    tm_date.tm_isdst = -1;

    time_t t = mktime(&tm_date);
    if (t == -1) return 0;

    int days_since_saturday = (tm_date.tm_wday + 1) % 7;

    time_t start_t = t - (days_since_saturday * 24 * 60 * 60);
    time_t end_t = start_t + (6 * 24 * 60 * 60);

    struct tm *start_tm = localtime(&start_t);
    int sy = start_tm->tm_year + 1900;
    int sm = start_tm->tm_mon + 1;
    int sd = start_tm->tm_mday;

    struct tm *end_tm = localtime(&end_t);
    int ey = end_tm->tm_year + 1900;
    int em = end_tm->tm_mon + 1;
    int ed = end_tm->tm_mday;

    *start_int = sy * 10000 + sm * 100 + sd;
    *end_int = ey * 10000 + em * 100 + ed;

    snprintf(label_text, label_size, "Report Range: %04d-%02d-%02d to %04d-%02d-%02d", sy, sm, sd, ey, em, ed);

    return 1;
}

static int get_selected_range(int *start_int, int *end_int, char *label_text, int label_size)
{
    int selected = gtk_combo_box_get_active(GTK_COMBO_BOX(range_type_combo));

    const char *date = gtk_entry_get_text(GTK_ENTRY(date_entry));
    const char *month = gtk_entry_get_text(GTK_ENTRY(month_entry));
    const char *start_date = gtk_entry_get_text(GTK_ENTRY(start_date_entry));
    const char *end_date = gtk_entry_get_text(GTK_ENTRY(end_date_entry));

    if (selected == 0) {
        int d = parse_date_to_int(date);
        if (d == 0) return 0;

        *start_int = d;
        *end_int = d;
        snprintf(label_text, label_size, "Report Range: %s", date);
        return 1;
    }

    if (selected == 1) {
        return calculate_week_range(date, start_int, end_int, label_text, label_size);
    }

    if (selected == 2) {
        int y, m;
        if (sscanf(month, "%d-%d", &y, &m) != 2) return 0;

        *start_int = y * 10000 + m * 100 + 1;
        *end_int = y * 10000 + m * 100 + 31;

        snprintf(label_text, label_size, "Report Range: Month %04d-%02d", y, m);
        return 1;
    }

    if (selected == 3) {
        int s = parse_date_to_int(start_date);
        int e = parse_date_to_int(end_date);

        if (s == 0 || e == 0 || s > e) return 0;

        *start_int = s;
        *end_int = e;

        snprintf(label_text, label_size, "Report Range: %s to %s", start_date, end_date);
        return 1;
    }

    return 0;
}

/* Detailed history window for one Case ID */
static void show_case_history(const char *case_id)
{
    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) return;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Case History");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    char title_text[256];
    snprintf(title_text, sizeof(title_text), "<span size='15000' weight='bold'>History for Case ID: %s</span>", case_id);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), title_text);
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 5);

    GtkWidget *summary = gtk_label_new("");
    gtk_widget_set_halign(summary, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), summary, FALSE, FALSE, 5);

    GtkListStore *store = gtk_list_store_new(
        6,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING
    );

    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview), GTK_TREE_VIEW_GRID_LINES_BOTH);

    const char *headers[] = {"Date", "Name", "Age Group", "Sex", "Type", "FCN"};

    for (int i = 0; i < 6; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(headers[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), treeview);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 5);

    const char *sql =
        "SELECT date, name, age_group, sex, type, fcn "
        "FROM sessions "
        "WHERE case_id = ? "
        "ORDER BY date ASC, id ASC;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, case_id, -1, SQLITE_TRANSIENT);

        int total = 0;
        char name_text[256] = "";
        char latest_type[128] = "";
        char fcn_text[256] = "";

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *date = (const char *)sqlite3_column_text(stmt, 0);
            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            const char *age = (const char *)sqlite3_column_text(stmt, 2);
            const char *sex = (const char *)sqlite3_column_text(stmt, 3);
            const char *type = (const char *)sqlite3_column_text(stmt, 4);
            const char *fcn = (const char *)sqlite3_column_text(stmt, 5);

            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(
                store, &iter,
                0, date ? date : "",
                1, name ? name : "",
                2, age ? age : "",
                3, sex ? sex : "",
                4, type ? type : "",
                5, fcn ? fcn : "",
                -1
            );

            total++;

            if (name) strncpy(name_text, name, sizeof(name_text) - 1);
            if (type) strncpy(latest_type, type, sizeof(latest_type) - 1);
            if (fcn) strncpy(fcn_text, fcn, sizeof(fcn_text) - 1);
        }

        char summary_text[700];
        snprintf(
            summary_text,
            sizeof(summary_text),
            "Name: %s\nTotal Sessions: %d\nLatest Case Status: %s\nFCN: %s",
            name_text,
            total,
            latest_type,
            fcn_text
        );

        gtk_label_set_text(GTK_LABEL(summary), summary_text);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    gtk_widget_show_all(window);
}

static void on_report_row_activated(GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gchar *case_id = NULL;

        gtk_tree_model_get(model, &iter, R_COL_CASE_ID, &case_id, -1);

        if (
            case_id &&
            strcmp(case_id, "GRAND TOTAL SESSIONS") != 0 &&
            strcmp(case_id, "NEW CASES") != 0 &&
            strcmp(case_id, "OLD CASES") != 0
        ) {
            show_case_history(case_id);
        }

        if (case_id) g_free(case_id);
    }
}

static void show_type_list_window(const char *type_filter)
{
    if (!report_has_generated) {
        show_message(GTK_WINDOW(report_window), "No Report", "Please generate a report first using View Record.", GTK_MESSAGE_WARNING);
        return;
    }

    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) return;

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    char window_title[256];
    snprintf(window_title, sizeof(window_title), "%s List", type_filter);
    gtk_window_set_title(GTK_WINDOW(window), window_title);

    gtk_window_set_default_size(GTK_WINDOW(window), 650, 500);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 12);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    char title_markup[300];
    snprintf(title_markup, sizeof(title_markup), "<span size='15000' weight='bold'>%s within Selected Date Range</span>", type_filter);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), title_markup);
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 5);

    GtkListStore *store = gtk_list_store_new(
        4,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING
    );

    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview), GTK_TREE_VIEW_GRID_LINES_BOTH);

    const char *headers[] = {"Case ID", "Name", "Sex", "Age Group"};

    for (int i = 0; i < 4; i++) {
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(headers[i], renderer, "text", i, NULL);
        gtk_tree_view_column_set_resizable(column, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    }

    const char *sql =
        "SELECT case_id, name, sex, age_group, MIN(date) "
        "FROM sessions "
        "WHERE type = ? "
        "GROUP BY case_id, name, sex, age_group "
        "ORDER BY case_id ASC;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, type_filter, -1, SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *case_id = (const char *)sqlite3_column_text(stmt, 0);
            const char *name = (const char *)sqlite3_column_text(stmt, 1);
            const char *sex = (const char *)sqlite3_column_text(stmt, 2);
            const char *age = (const char *)sqlite3_column_text(stmt, 3);

            /*
             * Check date range manually because saved dates may be 2026-5-5
             * or 2026-05-05. parse_date_to_int supports both.
             */
            sqlite3_stmt *date_stmt;
            const char *date_sql = "SELECT date FROM sessions WHERE case_id = ? AND type = ?;";

            int include = 0;

            if (sqlite3_prepare_v2(db, date_sql, -1, &date_stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_text(date_stmt, 1, case_id, -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(date_stmt, 2, type_filter, -1, SQLITE_TRANSIENT);

                while (sqlite3_step(date_stmt) == SQLITE_ROW) {
                    const char *date = (const char *)sqlite3_column_text(date_stmt, 0);
                    int d = parse_date_to_int(date);

                    if (d >= current_start_int && d <= current_end_int) {
                        include = 1;
                        break;
                    }
                }

                sqlite3_finalize(date_stmt);
            }

            if (include) {
                GtkTreeIter iter;
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(
                    store, &iter,
                    0, case_id ? case_id : "",
                    1, name ? name : "",
                    2, sex ? sex : "",
                    3, age ? age : "",
                    -1
                );
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), treeview);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 5);

    g_signal_connect(treeview, "row-activated", G_CALLBACK(on_report_row_activated), NULL);

    GtkWidget *note = gtk_label_new("Double-click a Case ID row to open full history.");
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), note, FALSE, FALSE, 5);

    gtk_widget_show_all(window);
}

static void on_new_clicked(GtkWidget *button, gpointer user_data)
{
    show_type_list_window("New Case");
}

static void on_old_clicked(GtkWidget *button, gpointer user_data)
{
    show_type_list_window("Old Case");
}

static void on_export_csv_clicked(GtkWidget *button, gpointer user_data)
{
    if (!report_has_generated) {
        show_message(
            GTK_WINDOW(report_window),
            "No Report",
            "Please generate a report first using View Record before exporting CSV.",
            GTK_MESSAGE_WARNING
        );
        return;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Save CSV Report",
        GTK_WINDOW(report_window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        "_Save",
        GTK_RESPONSE_ACCEPT,
        NULL
    );

    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "beneficiary_session_report.csv");
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    int response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_ACCEPT) {
        char *file_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        char error_message[256] = "";

        int success = export_report_csv(
            current_start_int,
            current_end_int,
            file_path,
            error_message,
            sizeof(error_message)
        );

        if (success) {
            show_message(
                GTK_WINDOW(report_window),
                "CSV Exported",
                "CSV report exported successfully.",
                GTK_MESSAGE_INFO
            );
        } else {
            show_message(
                GTK_WINDOW(report_window),
                "CSV Export Failed",
                error_message,
                GTK_MESSAGE_ERROR
            );
        }

        g_free(file_path);
    }

    gtk_widget_destroy(dialog);
}

static void generate_report(void)
{
    int start_int;
    int end_int;
    char label_text[256];

    if (!get_selected_range(&start_int, &end_int, label_text, sizeof(label_text))) {
        show_message(
            GTK_WINDOW(report_window),
            "Invalid Date Range",
            "Please enter a valid date range.\n\nDate format: YYYY-MM-DD\nMonth format: YYYY-MM",
            GTK_MESSAGE_WARNING
        );
        return;
    }

    current_start_int = start_int;
    current_end_int = end_int;
    report_has_generated = 1;

    clear_report_data();
    gtk_label_set_text(GTK_LABEL(range_label), label_text);

    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) {
        show_message(GTK_WINDOW(report_window), "Database Error", "Could not open database.", GTK_MESSAGE_ERROR);
        return;
    }

    const char *sql = "SELECT date, case_id, age_group, sex, type FROM sessions;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        show_message(GTK_WINDOW(report_window), "Database Error", "Could not read records.", GTK_MESSAGE_ERROR);
        return;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *date = (const char *)sqlite3_column_text(stmt, 0);
        const char *case_id = (const char *)sqlite3_column_text(stmt, 1);
        const char *age_group = (const char *)sqlite3_column_text(stmt, 2);
        const char *sex = (const char *)sqlite3_column_text(stmt, 3);
        const char *type = (const char *)sqlite3_column_text(stmt, 4);

        int record_date = parse_date_to_int(date);

        if (record_date < start_int || record_date > end_int) continue;

        int aidx = age_index(age_group);
        if (aidx < 0) continue;

        if (strcmp(sex, "Male") == 0) {
            int idx = find_or_add_case(male_cases, &male_case_count, case_id);

            if (idx >= 0) {
                male_cases[idx].counts[aidx]++;
                male_totals[aidx]++;
            }

            if (strcmp(type, "New Case") == 0 && !already_counted_new(counted_new_male, counted_new_male_count, case_id)) {
                male_new_cases[aidx]++;
                mark_new_counted(counted_new_male, &counted_new_male_count, case_id);
            }

            if (strcmp(type, "Old Case") == 0 && !already_counted_new(counted_old_male, counted_old_male_count, case_id)) {
                male_old_cases[aidx]++;
                mark_new_counted(counted_old_male, &counted_old_male_count, case_id);
            }
        }

        if (strcmp(sex, "Female") == 0) {
            int idx = find_or_add_case(female_cases, &female_case_count, case_id);

            if (idx >= 0) {
                female_cases[idx].counts[aidx]++;
                female_totals[aidx]++;
            }

            if (strcmp(type, "New Case") == 0 && !already_counted_new(counted_new_female, counted_new_female_count, case_id)) {
                female_new_cases[aidx]++;
                mark_new_counted(counted_new_female, &counted_new_female_count, case_id);
            }

            if (strcmp(type, "Old Case") == 0 && !already_counted_new(counted_old_female, counted_old_female_count, case_id)) {
                female_old_cases[aidx]++;
                mark_new_counted(counted_old_female, &counted_old_female_count, case_id);
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    fill_report_table();
}

static void on_view_record_clicked(GtkWidget *button, gpointer user_data)
{
    generate_report();
}

static void on_report_window_destroy(GtkWidget *widget, gpointer data)
{
    report_window = NULL;
}

void create_report_window(void)
{
    if (report_window != NULL) {
        gtk_window_present(GTK_WINDOW(report_window));
        return;
    }

    report_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(report_window), "Report - Beneficiary Session Management");
    gtk_window_set_default_size(GTK_WINDOW(report_window), 1150, 720);
    gtk_window_set_position(GTK_WINDOW(report_window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(report_window), 12);

    g_signal_connect(report_window, "destroy", G_CALLBACK(on_report_window_destroy), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(report_window), main_box);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='16000' weight='bold'>Session Report</span>");
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 5);

    GtkWidget *filter_frame = gtk_frame_new("Select Report Date Range");
    gtk_box_pack_start(GTK_BOX(main_box), filter_frame, FALSE, FALSE, 5);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_container_add(GTK_CONTAINER(filter_frame), grid);

    range_type_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(range_type_combo), "Specific Date");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(range_type_combo), "Week - Saturday to Friday");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(range_type_combo), "Month");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(range_type_combo), "Custom Date Range");
    gtk_combo_box_set_active(GTK_COMBO_BOX(range_type_combo), 0);

    date_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(date_entry), "YYYY-MM-DD");

    month_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(month_entry), "YYYY-MM");

    start_date_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(start_date_entry), "YYYY-MM-DD");

    end_date_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(end_date_entry), "YYYY-MM-DD");

    GtkWidget *view_button = gtk_button_new_with_label("View Record");
    GtkWidget *new_button = gtk_button_new_with_label("New");
    GtkWidget *old_button = gtk_button_new_with_label("Old");
    GtkWidget *export_csv_button = gtk_button_new_with_label("Export CSV");

    gtk_grid_attach(GTK_GRID(grid), create_label("Range Type:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), range_type_combo, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), create_label("Date / Week Date:"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), date_entry, 3, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), view_button, 4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), new_button, 5, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), old_button, 6, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), export_csv_button, 7, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), create_label("Month:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), month_entry, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), create_label("Start Date:"), 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), start_date_entry, 3, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), create_label("End Date:"), 4, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), end_date_entry, 5, 1, 1, 1);

    g_signal_connect(view_button, "clicked", G_CALLBACK(on_view_record_clicked), NULL);
    g_signal_connect(new_button, "clicked", G_CALLBACK(on_new_clicked), NULL);
    g_signal_connect(old_button, "clicked", G_CALLBACK(on_old_clicked), NULL);
    g_signal_connect(export_csv_button, "clicked", G_CALLBACK(on_export_csv_clicked), NULL);

    range_label = gtk_label_new("Report Range: Not generated yet");
    gtk_widget_set_halign(range_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), range_label, FALSE, FALSE, 5);

    GtkWidget *tables_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), tables_box, TRUE, TRUE, 5);

    GtkWidget *male_frame = gtk_frame_new("Male");
    GtkWidget *female_frame = gtk_frame_new("Female");

    gtk_box_pack_start(GTK_BOX(tables_box), male_frame, TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(tables_box), female_frame, TRUE, TRUE, 5);

    GtkWidget *male_tree = create_report_tree(&male_store, &male_treeview);
    GtkWidget *female_tree = create_report_tree(&female_store, &female_treeview);

    gtk_container_add(GTK_CONTAINER(male_frame), male_tree);
    gtk_container_add(GTK_CONTAINER(female_frame), female_tree);

    g_signal_connect(male_treeview, "row-activated", G_CALLBACK(on_report_row_activated), NULL);
    g_signal_connect(female_treeview, "row-activated", G_CALLBACK(on_report_row_activated), NULL);

    GtkWidget *note = gtk_label_new(
        "Double-click any Case ID to open full history. GRAND TOTAL SESSIONS counts visits. NEW CASES counts unique Case IDs marked New Case."
    );
    gtk_widget_set_halign(note, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(main_box), note, FALSE, FALSE, 5);

    gtk_widget_show_all(report_window);
}
