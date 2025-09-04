#ifndef SYSTRAY_WIDGET_H
#define SYSTRAY_WIDGET_H

#include <gtk/gtk.h>
#include <gio/gio.h>
#include "../config.h"

G_BEGIN_DECLS

#define SYSTRAY_TYPE_WIDGET (systray_widget_get_type())
G_DECLARE_FINAL_TYPE(SystrayWidget, systray_widget, SYSTRAY, WIDGET, GtkBox)

GtkWidget *systray_widget_new(PanelConfig *config);

G_END_DECLS

#endif // SYSTRAY_WIDGET_H
