#ifndef LAUNCHER_WIDGET_H
#define LAUNCHER_WIDGET_H

#include <gtk/gtk.h>
#include "../config.h"

G_BEGIN_DECLS

#define LAUNCHER_TYPE_WIDGET (launcher_widget_get_type())
G_DECLARE_FINAL_TYPE(LauncherWidget, launcher_widget, LAUNCHER, WIDGET, GtkBox)

GtkWidget *launcher_widget_new(PanelConfig *config);

G_END_DECLS

#endif // LAUNCHER_WIDGET_H
