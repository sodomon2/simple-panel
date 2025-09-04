#ifndef SHOWDESKTOP_WIDGET_H
#define SHOWDESKTOP_WIDGET_H

#include <gtk/gtk.h>
#include <glib.h>
#include "../config.h"

G_BEGIN_DECLS

#define SHOWDESKTOP_TYPE_WIDGET showdesktop_widget_get_type()
G_DECLARE_FINAL_TYPE(ShowDesktopWidget, showdesktop_widget, SHOWDESKTOP, WIDGET, GtkBox)

// Crear nuevo widget de show desktop
ShowDesktopWidget *showdesktop_widget_new(PanelConfig *config);

G_END_DECLS

#endif // SHOWDESKTOP_WIDGET_H
