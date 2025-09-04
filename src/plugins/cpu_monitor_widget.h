#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CPU_TYPE_MONITOR_WIDGET (cpu_monitor_widget_get_type())
G_DECLARE_FINAL_TYPE(CpuMonitorWidget, cpu_monitor_widget, CPU, MONITOR_WIDGET, GtkBox)

GtkWidget *cpu_monitor_widget_new(void);

G_END_DECLS
