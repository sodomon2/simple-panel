#ifndef PANEL_WINDOW_H
#define PANEL_WINDOW_H

#include <gtk/gtk.h>
// Incluimos la cabecera de la librería para wlr-layer-shell
#include <gtk4-layer-shell.h>

G_BEGIN_DECLS

// Macro para registrar nuestro nuevo tipo de widget
#define PANEL_TYPE_WINDOW (panel_window_get_type())

// Declaración del objeto PanelWindow que hereda de GtkApplicationWindow
G_DECLARE_FINAL_TYPE(PanelWindow, panel_window, PANEL, WINDOW, GtkApplicationWindow)

// Prototipo de la función para crear una nueva ventana de panel
GtkWidget *panel_window_new(GtkApplication *app);

G_END_DECLS

#endif // PANEL_WINDOW_H
