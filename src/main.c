#include <gtk/gtk.h>
#include "panel.h"
#include "i18n.h"

// Callback que se ejecuta cuando la aplicación se activa (inicia)
// Usamos G_GNUC_UNUSED para silenciar el aviso de parámetro no usado.
static void on_activate(GtkApplication *app, gpointer G_GNUC_UNUSED user_data) {
    // Crea una nueva instancia de nuestra ventana de panel
    GtkWidget *window = panel_window_new(app);

    // Muestra la ventana. gtk_widget_present fue eliminada en GTK4.
    // La forma correcta es usar gtk_window_present para un GtkWindow.
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    // Initialize internationalization
    i18n_init();

    // Crea una nueva aplicación GTK. El ID debe ser único.
    app = gtk_application_new("io.gitlab.sodomon.simple_panel", G_APPLICATION_DEFAULT_FLAGS);

    // Conecta la señal "activate" a nuestro callback on_activate
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    // Ejecuta la aplicación y guarda el estado de salida
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // Libera la memoria de la aplicación
    g_object_unref(app);

    return status;
}
