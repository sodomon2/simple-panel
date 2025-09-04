#ifndef TASKLIST_WIDGET_H
#define TASKLIST_WIDGET_H

#include <gtk/gtk.h>
#include <glib.h>
#include "../config.h"

G_BEGIN_DECLS

#define TASKLIST_TYPE_WIDGET tasklist_widget_get_type()
G_DECLARE_FINAL_TYPE(TasklistWidget, tasklist_widget, TASKLIST, WIDGET, GtkBox)

// Crear nuevo widget de tasklist
TasklistWidget *tasklist_widget_new(PanelConfig *config);

G_END_DECLS

#endif // TASKLIST_WIDGET_H
