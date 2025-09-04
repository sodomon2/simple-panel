#include "ram_monitor_widget.h"
#include "../i18n.h"
#include <stdio.h>

struct _RamMonitorWidget {
    GtkBox parent_instance;
    
    GtkWidget *progress_bar;
    guint timeout_id;
    
    // Datos de memoria
    double mem_total_gb;
    double mem_used_gb;
    double mem_free_gb;
    double mem_percent;
};

G_DEFINE_TYPE(RamMonitorWidget, ram_monitor_widget, GTK_TYPE_BOX)

static void read_memory_info(RamMonitorWidget *self) {
    FILE *file = fopen("/proc/meminfo", "r");
    if (!file) return;
    
    gulong mem_total = 0, mem_free = 0, mem_available = 0, buffers = 0, cached = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) continue;
        if (sscanf(line, "MemFree: %lu kB", &mem_free) == 1) continue;
        if (sscanf(line, "MemAvailable: %lu kB", &mem_available) == 1) continue;
        if (sscanf(line, "Buffers: %lu kB", &buffers) == 1) continue;
        if (sscanf(line, "Cached: %lu kB", &cached) == 1) continue;
    }
    fclose(file);
    
    if (mem_total > 0) {
        // Usar MemAvailable si está disponible, sino calcular
        gulong mem_used = mem_available > 0 ? 
            mem_total - mem_available : 
            mem_total - mem_free - buffers - cached;
            
        self->mem_total_gb = mem_total / (1024.0 * 1024.0);
        self->mem_used_gb = mem_used / (1024.0 * 1024.0);
        self->mem_free_gb = self->mem_total_gb - self->mem_used_gb;
        self->mem_percent = (mem_used * 100.0) / mem_total;
    }
}

static void update_tooltip(RamMonitorWidget *self) {
    gchar *tooltip = g_strdup_printf(
        _("RAM: %.1f GB / %.1f GB (%.1f%%)\n"
        "Used: %.1f GB\n"
        "Free: %.1f GB"),
        self->mem_used_gb, self->mem_total_gb, self->mem_percent,
        self->mem_used_gb, self->mem_free_gb
    );
    
    gtk_widget_set_tooltip_text(GTK_WIDGET(self), tooltip);
    g_free(tooltip);
}

static gboolean on_update_memory(gpointer user_data) {
    RamMonitorWidget *self = RAM_MONITOR_WIDGET(user_data);
    
    read_memory_info(self);
    
    // Actualizar progress bar
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(self->progress_bar), 
                                 self->mem_percent / 100.0);
    
    update_tooltip(self);
    
    return G_SOURCE_CONTINUE;
}

static void ram_monitor_widget_dispose(GObject *object) {
    RamMonitorWidget *self = RAM_MONITOR_WIDGET(object);
    
    if (self->timeout_id > 0) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
    
    G_OBJECT_CLASS(ram_monitor_widget_parent_class)->dispose(object);
}

static void ram_monitor_widget_init(RamMonitorWidget *self) {
    // Orientación vertical
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);
    
    // Crear progress bar vertical
    self->progress_bar = gtk_progress_bar_new();
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self->progress_bar), GTK_ORIENTATION_VERTICAL);
    gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(self->progress_bar), TRUE); // De abajo hacia arriba
    gtk_widget_add_css_class(self->progress_bar, "ram-monitor-bar");
    gtk_box_append(GTK_BOX(self), self->progress_bar);
    
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
    
    // Actualización inmediata
    read_memory_info(self);
    update_tooltip(self);
    
    // Timer cada 2 segundos
    self->timeout_id = g_timeout_add_seconds(2, on_update_memory, self);
}

static void ram_monitor_widget_class_init(RamMonitorWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = ram_monitor_widget_dispose;
}

GtkWidget *ram_monitor_widget_new(void) {
    return g_object_new(RAM_TYPE_MONITOR_WIDGET, NULL);
}
