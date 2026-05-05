#include <gtk/gtk.h>
#include <sqlite3.h>
#include <string.h>
#include "ui.h"
#include "database.h"
#include "report.h"

/* Main form widgets */
static GtkWidget *date_entry;
static GtkWidget *case_id_entry;
static GtkWidget *name_entry;
static GtkWidget *age_group_combo;
static GtkWidget *sex_combo;
static GtkWidget *type_combo;
static GtkWidget *fcn_entry;

/* Record window widgets */
static GtkWidget *records_window = NULL;
static GtkWidget *records_treeview;
static GtkListStore *records_store;

static GtkWidget *filter_date_entry;
static GtkWidget *filter_case_id_entry;
static GtkWidget *filter_name_entry;
static GtkWidget *filter_age_group_combo;
static GtkWidget *filter_sex_combo;
static GtkWidget *filter_type_combo;
static GtkWidget *filter_fcn_entry;

/* Edit form widgets */
static GtkWidget *edit_id_entry;
static GtkWidget *edit_date_entry;
static GtkWidget *edit_case_id_entry;
static GtkWidget *edit_name_entry;
static GtkWidget *edit_age_group_combo;
static GtkWidget *edit_sex_combo;
static GtkWidget *edit_type_combo;
static GtkWidget *edit_fcn_entry;

/* Table columns */
enum {
    COL_ID,
    COL_DATE,
    COL_CASE_ID,
    COL_NAME,
    COL_AGE_GROUP,
    COL_SEX,
    COL_TYPE,
    COL_FCN,
    NUM_COLS
};

static void show_message(GtkWindow *parent, const char *title, const char *message, GtkMessageType type)
{
    GtkWidget *dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL,
        type,
        GTK_BUTTONS_OK,
        "%s",
        message
    );

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

static void set_combo_text(GtkWidget *combo, const char *text)
{
    if (text == NULL) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), -1);
        return;
    }

    if (strcmp(text, "5-11") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    else if (strcmp(text, "12-17") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 1);
    else if (strcmp(text, "18-59") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 2);
    else if (strcmp(text, "60+") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 3);
    else if (strcmp(text, "Male") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    else if (strcmp(text, "Female") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 1);
    else if (strcmp(text, "New Case") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    else if (strcmp(text, "Old Case") == 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 1);
    else gtk_combo_box_set_active(GTK_COMBO_BOX(combo), -1);
}

static GtkWidget *create_age_group_combo(void)
{
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "5-11");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "12-17");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "18-59");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "60+");
    return combo;
}

static GtkWidget *create_sex_combo(void)
{
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Male");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Female");
    return combo;
}

static GtkWidget *create_type_combo(void)
{
    GtkWidget *combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "New Case");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), "Old Case");
    return combo;
}

static void clear_main_form(void)
{
    gtk_entry_set_text(GTK_ENTRY(date_entry), "");
    gtk_entry_set_text(GTK_ENTRY(case_id_entry), "");
    gtk_entry_set_text(GTK_ENTRY(name_entry), "");
    gtk_entry_set_text(GTK_ENTRY(fcn_entry), "");

    gtk_combo_box_set_active(GTK_COMBO_BOX(age_group_combo), -1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sex_combo), -1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(type_combo), -1);
}

static int validate_required(
    GtkWindow *parent,
    const char *date,
    const char *case_id,
    const char *name,
    const char *age_group,
    const char *sex,
    const char *type
)
{
    if (
        date == NULL || date[0] == '\0' ||
        case_id == NULL || case_id[0] == '\0' ||
        name == NULL || name[0] == '\0' ||
        age_group == NULL ||
        sex == NULL ||
        type == NULL
    ) {
        show_message(
            parent,
            "Validation Error",
            "Please fill Date, Case ID, Beneficiary Name, Age Group, Sex, and Type.",
            GTK_MESSAGE_WARNING
        );
        return 0;
    }

    return 1;
}

static void on_save_clicked(GtkWidget *button, gpointer user_data)
{
    GtkWindow *parent = GTK_WINDOW(user_data);

    const char *date = gtk_entry_get_text(GTK_ENTRY(date_entry));
    const char *case_id = gtk_entry_get_text(GTK_ENTRY(case_id_entry));
    const char *name = gtk_entry_get_text(GTK_ENTRY(name_entry));
    const char *fcn = gtk_entry_get_text(GTK_ENTRY(fcn_entry));

    gchar *age_group = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(age_group_combo));
    gchar *sex = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(sex_combo));
    gchar *type = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(type_combo));

    if (!validate_required(parent, date, case_id, name, age_group, sex, type)) {
        if (age_group) g_free(age_group);
        if (sex) g_free(sex);
        if (type) g_free(type);
        return;
    }

    int success = insert_session(date, case_id, name, age_group, sex, type, fcn);

    if (success) {
        show_message(parent, "Saved", "Session record saved successfully.", GTK_MESSAGE_INFO);
        clear_main_form();
    } else {
        show_message(parent, "Database Error", "Could not save the session record.", GTK_MESSAGE_ERROR);
    }

    g_free(age_group);
    g_free(sex);
    g_free(type);
}

static void on_clear_clicked(GtkWidget *button, gpointer user_data)
{
    clear_main_form();
}

/* Adds one sortable text column to the records table */
static void add_text_column(const char *title, int column_id)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        title,
        renderer,
        "text",
        column_id,
        NULL
    );

    gtk_tree_view_column_set_sort_column_id(column, column_id);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 90);

    gtk_tree_view_append_column(GTK_TREE_VIEW(records_treeview), column);
}

/* Loads records using filters */
static void load_records(void)
{
    sqlite3 *db = open_database();
    sqlite3_stmt *stmt;

    if (db == NULL) {
        return;
    }

    gtk_list_store_clear(records_store);

    const char *date = gtk_entry_get_text(GTK_ENTRY(filter_date_entry));
    const char *case_id = gtk_entry_get_text(GTK_ENTRY(filter_case_id_entry));
    const char *name = gtk_entry_get_text(GTK_ENTRY(filter_name_entry));
    const char *fcn = gtk_entry_get_text(GTK_ENTRY(filter_fcn_entry));

    gchar *age_group = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(filter_age_group_combo));
    gchar *sex = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(filter_sex_combo));
    gchar *type = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(filter_type_combo));

    const char *sql =
        "SELECT id, date, case_id, name, age_group, sex, type, fcn "
        "FROM sessions "
        "WHERE date LIKE ? "
        "AND case_id LIKE ? "
        "AND name LIKE ? "
        "AND age_group LIKE ? "
        "AND sex LIKE ? "
        "AND type LIKE ? "
        "AND fcn LIKE ? "
        "ORDER BY date DESC, id DESC;";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        printf("Load records prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    char date_filter[256];
    char case_id_filter[256];
    char name_filter[256];
    char age_group_filter[256];
    char sex_filter[256];
    char type_filter[256];
    char fcn_filter[256];

    snprintf(date_filter, sizeof(date_filter), "%%%s%%", date);
    snprintf(case_id_filter, sizeof(case_id_filter), "%%%s%%", case_id);
    snprintf(name_filter, sizeof(name_filter), "%%%s%%", name);
    snprintf(age_group_filter, sizeof(age_group_filter), "%%%s%%", age_group ? age_group : "");
    snprintf(sex_filter, sizeof(sex_filter), "%%%s%%", sex ? sex : "");
    snprintf(type_filter, sizeof(type_filter), "%%%s%%", type ? type : "");
    snprintf(fcn_filter, sizeof(fcn_filter), "%%%s%%", fcn);

    sqlite3_bind_text(stmt, 1, date_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, case_id_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, age_group_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, sex_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, type_filter, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, fcn_filter, -1, SQLITE_TRANSIENT);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GtkTreeIter iter;

        gtk_list_store_append(records_store, &iter);

        gtk_list_store_set(
            records_store,
            &iter,
            COL_ID, sqlite3_column_int(stmt, 0),
            COL_DATE, sqlite3_column_text(stmt, 1),
            COL_CASE_ID, sqlite3_column_text(stmt, 2),
            COL_NAME, sqlite3_column_text(stmt, 3),
            COL_AGE_GROUP, sqlite3_column_text(stmt, 4),
            COL_SEX, sqlite3_column_text(stmt, 5),
            COL_TYPE, sqlite3_column_text(stmt, 6),
            COL_FCN, sqlite3_column_text(stmt, 7),
            -1
        );
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (age_group) g_free(age_group);
    if (sex) g_free(sex);
    if (type) g_free(type);
}

static void clear_edit_form(void)
{
    gtk_entry_set_text(GTK_ENTRY(edit_id_entry), "");
    gtk_entry_set_text(GTK_ENTRY(edit_date_entry), "");
    gtk_entry_set_text(GTK_ENTRY(edit_case_id_entry), "");
    gtk_entry_set_text(GTK_ENTRY(edit_name_entry), "");
    gtk_entry_set_text(GTK_ENTRY(edit_fcn_entry), "");

    gtk_combo_box_set_active(GTK_COMBO_BOX(edit_age_group_combo), -1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(edit_sex_combo), -1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(edit_type_combo), -1);
}

/* When user selects row, load it into edit form */
static void on_record_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        int id;
        gchar *date;
        gchar *case_id;
        gchar *name;
        gchar *age_group;
        gchar *sex;
        gchar *type;
        gchar *fcn;

        gtk_tree_model_get(
            model,
            &iter,
            COL_ID, &id,
            COL_DATE, &date,
            COL_CASE_ID, &case_id,
            COL_NAME, &name,
            COL_AGE_GROUP, &age_group,
            COL_SEX, &sex,
            COL_TYPE, &type,
            COL_FCN, &fcn,
            -1
        );

        char id_text[32];
        snprintf(id_text, sizeof(id_text), "%d", id);

        gtk_entry_set_text(GTK_ENTRY(edit_id_entry), id_text);
        gtk_entry_set_text(GTK_ENTRY(edit_date_entry), date ? date : "");
        gtk_entry_set_text(GTK_ENTRY(edit_case_id_entry), case_id ? case_id : "");
        gtk_entry_set_text(GTK_ENTRY(edit_name_entry), name ? name : "");
        gtk_entry_set_text(GTK_ENTRY(edit_fcn_entry), fcn ? fcn : "");

        set_combo_text(edit_age_group_combo, age_group);
        set_combo_text(edit_sex_combo, sex);
        set_combo_text(edit_type_combo, type);

        g_free(date);
        g_free(case_id);
        g_free(name);
        g_free(age_group);
        g_free(sex);
        g_free(type);
        g_free(fcn);
    }
}

static void on_search_clicked(GtkWidget *button, gpointer user_data)
{
    load_records();
}

static void on_refresh_clicked(GtkWidget *button, gpointer user_data)
{
    load_records();
}

static void on_clear_filter_clicked(GtkWidget *button, gpointer user_data)
{
    gtk_entry_set_text(GTK_ENTRY(filter_date_entry), "");
    gtk_entry_set_text(GTK_ENTRY(filter_case_id_entry), "");
    gtk_entry_set_text(GTK_ENTRY(filter_name_entry), "");
    gtk_entry_set_text(GTK_ENTRY(filter_fcn_entry), "");

    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_age_group_combo), -1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_sex_combo), -1);
    gtk_combo_box_set_active(GTK_COMBO_BOX(filter_type_combo), -1);

    load_records();
}

static void on_update_clicked(GtkWidget *button, gpointer user_data)
{
    GtkWindow *parent = GTK_WINDOW(user_data);

    const char *id_text = gtk_entry_get_text(GTK_ENTRY(edit_id_entry));

    if (id_text == NULL || id_text[0] == '\0') {
        show_message(parent, "No Record Selected", "Please select a record from the table first.", GTK_MESSAGE_WARNING);
        return;
    }

    int id = atoi(id_text);

    const char *date = gtk_entry_get_text(GTK_ENTRY(edit_date_entry));
    const char *case_id = gtk_entry_get_text(GTK_ENTRY(edit_case_id_entry));
    const char *name = gtk_entry_get_text(GTK_ENTRY(edit_name_entry));
    const char *fcn = gtk_entry_get_text(GTK_ENTRY(edit_fcn_entry));

    gchar *age_group = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(edit_age_group_combo));
    gchar *sex = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(edit_sex_combo));
    gchar *type = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(edit_type_combo));

    if (!validate_required(parent, date, case_id, name, age_group, sex, type)) {
        if (age_group) g_free(age_group);
        if (sex) g_free(sex);
        if (type) g_free(type);
        return;
    }

    int success = update_session(id, date, case_id, name, age_group, sex, type, fcn);

    if (success) {
        show_message(parent, "Updated", "Record updated successfully.", GTK_MESSAGE_INFO);
        clear_edit_form();
        load_records();
    } else {
        show_message(parent, "Update Failed", "Could not update the selected record.", GTK_MESSAGE_ERROR);
    }

    g_free(age_group);
    g_free(sex);
    g_free(type);
}

static void on_delete_clicked(GtkWidget *button, gpointer user_data)
{
    GtkWindow *parent = GTK_WINDOW(user_data);

    const char *id_text = gtk_entry_get_text(GTK_ENTRY(edit_id_entry));

    if (id_text == NULL || id_text[0] == '\0') {
        show_message(parent, "No Record Selected", "Please select a record from the table first.", GTK_MESSAGE_WARNING);
        return;
    }

    GtkWidget *dialog = gtk_message_dialog_new(
        parent,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "Are you sure you want to delete this record?"
    );

    gtk_window_set_title(GTK_WINDOW(dialog), "Confirm Delete");

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response != GTK_RESPONSE_YES) {
        return;
    }

    int id = atoi(id_text);

    if (delete_session(id)) {
        show_message(parent, "Deleted", "Record deleted successfully.", GTK_MESSAGE_INFO);
        clear_edit_form();
        load_records();
    } else {
        show_message(parent, "Delete Failed", "Could not delete the selected record.", GTK_MESSAGE_ERROR);
    }
}

static void on_records_window_destroy(GtkWidget *widget, gpointer data)
{
    records_window = NULL;
}

static void create_records_window(void)
{
    if (records_window != NULL) {
        gtk_window_present(GTK_WINDOW(records_window));
        return;
    }

    records_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(records_window), "Records - Beneficiary Session Management");
    gtk_window_set_default_size(GTK_WINDOW(records_window), 1100, 650);
    gtk_window_set_position(GTK_WINDOW(records_window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(records_window), 12);

    g_signal_connect(records_window, "destroy", G_CALLBACK(on_records_window_destroy), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(records_window), main_box);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='15000' weight='bold'>Record View, Search, Sort, Edit and Delete</span>");
    gtk_box_pack_start(GTK_BOX(main_box), title, FALSE, FALSE, 5);

    /* Filter area */
    GtkWidget *filter_frame = gtk_frame_new("Search / Filter Records");
    gtk_box_pack_start(GTK_BOX(main_box), filter_frame, FALSE, FALSE, 5);

    GtkWidget *filter_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(filter_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(filter_grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(filter_grid), 10);
    gtk_container_add(GTK_CONTAINER(filter_frame), filter_grid);

    filter_date_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(filter_date_entry), "YYYY-MM-DD or part");

    filter_case_id_entry = gtk_entry_new();
    filter_name_entry = gtk_entry_new();
    filter_fcn_entry = gtk_entry_new();

    filter_age_group_combo = create_age_group_combo();
    filter_sex_combo = create_sex_combo();
    filter_type_combo = create_type_combo();

    gtk_grid_attach(GTK_GRID(filter_grid), create_label("Date:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), filter_date_entry, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(filter_grid), create_label("Case ID:"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), filter_case_id_entry, 3, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(filter_grid), create_label("Name:"), 4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), filter_name_entry, 5, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(filter_grid), create_label("Age:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), filter_age_group_combo, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(filter_grid), create_label("Sex:"), 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), filter_sex_combo, 3, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(filter_grid), create_label("Type:"), 4, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), filter_type_combo, 5, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(filter_grid), create_label("FCN:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), filter_fcn_entry, 1, 2, 1, 1);

    GtkWidget *search_button = gtk_button_new_with_label("Search");
    GtkWidget *clear_filter_button = gtk_button_new_with_label("Clear Search");
    GtkWidget *refresh_button = gtk_button_new_with_label("Refresh");

    gtk_grid_attach(GTK_GRID(filter_grid), search_button, 3, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), clear_filter_button, 4, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(filter_grid), refresh_button, 5, 2, 1, 1);

    g_signal_connect(search_button, "clicked", G_CALLBACK(on_search_clicked), NULL);
    g_signal_connect(clear_filter_button, "clicked", G_CALLBACK(on_clear_filter_clicked), NULL);
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), NULL);

    /* Records table */
    records_store = gtk_list_store_new(
        NUM_COLS,
        G_TYPE_INT,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING
    );

    records_treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(records_store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(records_treeview), TRUE);
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(records_treeview), GTK_TREE_VIEW_GRID_LINES_BOTH);

    add_text_column("ID", COL_ID);
    add_text_column("Date", COL_DATE);
    add_text_column("Case ID", COL_CASE_ID);
    add_text_column("Name", COL_NAME);
    add_text_column("Age Group", COL_AGE_GROUP);
    add_text_column("Sex", COL_SEX);
    add_text_column("Type", COL_TYPE);
    add_text_column("FCN", COL_FCN);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(records_treeview));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(selection, "changed", G_CALLBACK(on_record_selection_changed), NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), records_treeview);
    gtk_box_pack_start(GTK_BOX(main_box), scroll, TRUE, TRUE, 5);

    /* Edit area */
    GtkWidget *edit_frame = gtk_frame_new("Edit Selected Record");
    gtk_box_pack_start(GTK_BOX(main_box), edit_frame, FALSE, FALSE, 5);

    GtkWidget *edit_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(edit_grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(edit_grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(edit_grid), 10);
    gtk_container_add(GTK_CONTAINER(edit_frame), edit_grid);

    edit_id_entry = gtk_entry_new();
    gtk_editable_set_editable(GTK_EDITABLE(edit_id_entry), FALSE);

    edit_date_entry = gtk_entry_new();
    edit_case_id_entry = gtk_entry_new();
    edit_name_entry = gtk_entry_new();
    edit_fcn_entry = gtk_entry_new();

    edit_age_group_combo = create_age_group_combo();
    edit_sex_combo = create_sex_combo();
    edit_type_combo = create_type_combo();

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("ID:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_id_entry, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("Date:"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_date_entry, 3, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("Case ID:"), 4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_case_id_entry, 5, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("Name:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_name_entry, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("Age:"), 2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_age_group_combo, 3, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("Sex:"), 4, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_sex_combo, 5, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("Type:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_type_combo, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(edit_grid), create_label("FCN:"), 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), edit_fcn_entry, 3, 2, 1, 1);

    GtkWidget *update_button = gtk_button_new_with_label("Update Selected Record");
    GtkWidget *delete_button = gtk_button_new_with_label("Delete Selected Record");
    GtkWidget *clear_edit_button = gtk_button_new_with_label("Clear Edit Form");

    gtk_grid_attach(GTK_GRID(edit_grid), update_button, 3, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), delete_button, 4, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(edit_grid), clear_edit_button, 5, 3, 1, 1);

    g_signal_connect(update_button, "clicked", G_CALLBACK(on_update_clicked), records_window);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_clicked), records_window);
    g_signal_connect(clear_edit_button, "clicked", G_CALLBACK(clear_edit_form), NULL);

    load_records();

    gtk_widget_show_all(records_window);
}

static void on_view_records_clicked(GtkWidget *button, gpointer user_data)
{
    create_records_window();
}

static void on_view_report_clicked(GtkWidget *button, gpointer user_data)
{
    create_report_window();
}

static void on_destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

void create_main_window(void)
{
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(window), "Beneficiary Session Management");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 520);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(window), 20);

    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(
        GTK_LABEL(title_label),
        "<span size='18000' weight='bold'>Beneficiary Session Management</span>"
    );
    gtk_box_pack_start(GTK_BOX(main_box), title_label, FALSE, FALSE, 5);

    GtkWidget *form_grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(form_grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(form_grid), 12);
    gtk_box_pack_start(GTK_BOX(main_box), form_grid, TRUE, TRUE, 5);

    date_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(date_entry), "YYYY-MM-DD");

    case_id_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(case_id_entry), "Example: 12.58.58");

    name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "Beneficiary Name");

    age_group_combo = create_age_group_combo();
    sex_combo = create_sex_combo();
    type_combo = create_type_combo();

    fcn_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(fcn_entry), "FCN");

    gtk_grid_attach(GTK_GRID(form_grid), create_label("Date:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), date_entry, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(form_grid), create_label("Case ID:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), case_id_entry, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(form_grid), create_label("Beneficiary Name:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), name_entry, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(form_grid), create_label("Age Group:"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), age_group_combo, 1, 3, 1, 1);

    gtk_grid_attach(GTK_GRID(form_grid), create_label("Sex:"), 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), sex_combo, 1, 4, 1, 1);

    gtk_grid_attach(GTK_GRID(form_grid), create_label("Type:"), 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), type_combo, 1, 5, 1, 1);

    gtk_grid_attach(GTK_GRID(form_grid), create_label("FCN:"), 0, 6, 1, 1);
    gtk_grid_attach(GTK_GRID(form_grid), fcn_entry, 1, 6, 1, 1);

    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(main_box), button_box, FALSE, FALSE, 5);

    GtkWidget *save_button = gtk_button_new_with_label("Save");
    GtkWidget *clear_button = gtk_button_new_with_label("Clear");
    GtkWidget *view_records_button = gtk_button_new_with_label("View / Search Records");
    GtkWidget *view_report_button = gtk_button_new_with_label("View Report");

    gtk_box_pack_end(GTK_BOX(button_box), view_report_button, FALSE, FALSE, 5);
    gtk_box_pack_end(GTK_BOX(button_box), view_records_button, FALSE, FALSE, 5);
    gtk_box_pack_end(GTK_BOX(button_box), clear_button, FALSE, FALSE, 5);
    gtk_box_pack_end(GTK_BOX(button_box), save_button, FALSE, FALSE, 5);

    g_signal_connect(save_button, "clicked", G_CALLBACK(on_save_clicked), window);
    g_signal_connect(clear_button, "clicked", G_CALLBACK(on_clear_clicked), NULL);
    g_signal_connect(view_records_button, "clicked", G_CALLBACK(on_view_records_clicked), NULL);
    g_signal_connect(view_report_button, "clicked", G_CALLBACK(on_view_report_clicked), NULL);

    GtkWidget *footer_label = gtk_label_new("Developed by Raaz © 2026");
    gtk_box_pack_end(GTK_BOX(main_box), footer_label, FALSE, FALSE, 5);

    gtk_widget_show_all(window);
}
