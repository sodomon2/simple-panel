#ifndef CLOCK_WIDGET_H
#define CLOCK_WIDGET_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CLOCK_TYPE_WIDGET (clock_widget_get_type())
G_DECLARE_FINAL_TYPE(ClockWidget, clock_widget, CLOCK, WIDGET, GtkBox)

GtkWidget *clock_widget_new(void);

G_END_DECLS

#endif // CLOCK_WIDGET_H
