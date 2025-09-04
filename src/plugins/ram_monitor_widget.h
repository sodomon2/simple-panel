#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RAM_TYPE_MONITOR_WIDGET (ram_monitor_widget_get_type())
G_DECLARE_FINAL_TYPE(RamMonitorWidget, ram_monitor_widget, RAM, MONITOR_WIDGET, GtkBox)

GtkWidget *ram_monitor_widget_new(void);

G_END_DECLS
