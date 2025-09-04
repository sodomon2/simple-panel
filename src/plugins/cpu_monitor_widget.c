#include "cpu_monitor_widget.h"
#include "../i18n.h"
#include <stdio.h>

#define HISTORY_SIZE 30

struct _CpuMonitorWidget {
    GtkBox parent_instance;
    
    GtkWidget *drawing_area;
    guint timeout_id;
    
    // Historial de CPU para gráfica
    double cpu_history[HISTORY_SIZE];
    int history_index;
    
    // Datos CPU anteriores para cálculo
    gulong prev_total;
    gulong prev_idle;
    
    double current_cpu_percent;
};

G_DEFINE_TYPE(CpuMonitorWidget, cpu_monitor_widget, GTK_TYPE_BOX)

static void read_cpu_info(CpuMonitorWidget *self) {
    FILE *file = fopen("/proc/stat", "r");
    if (!file) return;
    
    gulong user, nice, system, idle, iowait, irq, softirq, steal;
    if (fscanf(file, "cpu %lu %lu %lu %lu %lu %lu %lu %lu", 
               &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) == 8) {
        
        gulong total = user + nice + system + idle + iowait + irq + softirq + steal;
        gulong idle_total = idle + iowait;
        
        if (self->prev_total > 0) {
            gulong total_diff = total - self->prev_total;
            gulong idle_diff = idle_total - self->prev_idle;
            
            if (total_diff > 0) {
                self->current_cpu_percent = ((double)(total_diff - idle_diff) / total_diff) * 100.0;
                if (self->current_cpu_percent < 0) self->current_cpu_percent = 0;
                if (self->current_cpu_percent > 100) self->current_cpu_percent = 100;
            }
        }
        
        self->prev_total = total;
        self->prev_idle = idle_total;
    }
    fclose(file);
}

static void update_tooltip(CpuMonitorWidget *self) {
    gchar *tooltip = g_strdup_printf("CPU: %.1f%%", self->current_cpu_percent);
    gtk_widget_set_tooltip_text(GTK_WIDGET(self), tooltip);
    g_free(tooltip);
}

static void draw_cpu_graph(GtkDrawingArea *area G_GNUC_UNUSED, cairo_t *cr, 
                          int width, int height, gpointer user_data) {
    CpuMonitorWidget *self = CPU_MONITOR_WIDGET(user_data);
    
    // Fondo
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    // Líneas de la gráfica
    cairo_set_line_width(cr, 1.0);
    cairo_set_source_rgba(cr, 0.0, 0.47, 0.84, 1.0); // Azul
    
    double x_step = (double)width / (HISTORY_SIZE - 1);
    
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        int idx1 = (self->history_index + i) % HISTORY_SIZE;
        int idx2 = (self->history_index + i + 1) % HISTORY_SIZE;
        
        double x1 = i * x_step;
        double x2 = (i + 1) * x_step;
        double y1 = height - (self->cpu_history[idx1] / 100.0) * height;
        double y2 = height - (self->cpu_history[idx2] / 100.0) * height;
        
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }
}

static gboolean on_update_cpu(gpointer user_data) {
    CpuMonitorWidget *self = CPU_MONITOR_WIDGET(user_data);
    
    read_cpu_info(self);
    
    // Actualizar historial
    self->cpu_history[self->history_index] = self->current_cpu_percent;
    self->history_index = (self->history_index + 1) % HISTORY_SIZE;
    
    update_tooltip(self);
    
    // Redibujar
    gtk_widget_queue_draw(self->drawing_area);
    
    return G_SOURCE_CONTINUE;
}

static void cpu_monitor_widget_dispose(GObject *object) {
    CpuMonitorWidget *self = CPU_MONITOR_WIDGET(object);
    
    if (self->timeout_id > 0) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
    
    G_OBJECT_CLASS(cpu_monitor_widget_parent_class)->dispose(object);
}

static void cpu_monitor_widget_init(CpuMonitorWidget *self) {
    // Inicializar historial
    for (int i = 0; i < HISTORY_SIZE; i++) {
        self->cpu_history[i] = 0.0;
    }
    self->history_index = 0;
    
    // Crear drawing area para la gráfica
    self->drawing_area = gtk_drawing_area_new();
    gtk_widget_add_css_class(self->drawing_area, "cpu-monitor-graph");
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(self->drawing_area), 
                                   draw_cpu_graph, self, NULL);
    gtk_box_append(GTK_BOX(self), self->drawing_area);
    
    // Cargar CSS desde GResource
    static gboolean styles_applied = FALSE;
    if (!styles_applied) {
        GtkCssProvider *css_provider = gtk_css_provider_new();
        gtk_css_provider_load_from_resource(css_provider, "/io/gitlab/sodomon/simple_panel/styles/monitor-styles.css");
        gtk_style_context_add_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(css_provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
        );
        g_object_unref(css_provider);
        styles_applied = TRUE;
    }
    
    // Primera lectura para inicializar
    read_cpu_info(self);
    
    // Timer cada segundo
    self->timeout_id = g_timeout_add_seconds(1, on_update_cpu, self);
}

static void cpu_monitor_widget_class_init(CpuMonitorWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = cpu_monitor_widget_dispose;
}

GtkWidget *cpu_monitor_widget_new(void) {
    return g_object_new(CPU_TYPE_MONITOR_WIDGET, NULL);
}
