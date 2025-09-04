#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NET_TYPE_MONITOR_WIDGET (net_monitor_widget_get_type())
G_DECLARE_FINAL_TYPE(NetMonitorWidget, net_monitor_widget, NET, MONITOR_WIDGET, GtkBox)

GtkWidget *net_monitor_widget_new(void);

G_END_DECLS
