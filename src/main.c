#include <gtk/gtk.h>
#include "ui.h"
#include "database.h"

int main(int argc, char *argv[])
{
    // Initialize GTK
    gtk_init(&argc, &argv);

    // Initialize database
    if (!init_database()) {
        g_print("Failed to initialize database.\n");
        return 1;
    }

    // Create UI
    create_main_window();

    // Start GTK loop
    gtk_main();

    return 0;
}