#include "net_monitor_widget.h"
#include "../i18n.h"
#include <stdio.h>
#include <string.h>

#define HISTORY_SIZE 30

struct _NetMonitorWidget {
    GtkBox parent_instance;
    
    GtkWidget *drawing_area;
    guint timeout_id;
    
    // Historial de tráfico de red
    double rx_history[HISTORY_SIZE];
    double tx_history[HISTORY_SIZE];
    int history_index;
    
    // Datos anteriores para calcular velocidad
    gulong prev_rx_bytes;
    gulong prev_tx_bytes;
    
    // Velocidades actuales (KB/s)
    double current_rx_speed;
    double current_tx_speed;
    double max_speed;
};

G_DEFINE_TYPE(NetMonitorWidget, net_monitor_widget, GTK_TYPE_BOX)

static void read_network_info(NetMonitorWidget *self) {
    FILE *file = fopen("/proc/net/dev", "r");
    if (!file) return;
    
    char line[256];
    gulong total_rx = 0, total_tx = 0;
    
    // Saltar las dos primeras líneas (headers)
    fgets(line, sizeof(line), file);
    fgets(line, sizeof(line), file);
    
    while (fgets(line, sizeof(line), file)) {
        char interface[16];
        gulong rx_bytes, tx_bytes;
        gulong dummy;
        
        // Parsear línea: interface: rx_bytes rx_packets ... tx_bytes tx_packets ...
        if (sscanf(line, "%15[^:]: %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", 
                   interface, &rx_bytes, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy,
                   &tx_bytes, &dummy) >= 10) {
            
            // Saltar loopback
            if (strcmp(interface, "lo") == 0) continue;
            
            total_rx += rx_bytes;
            total_tx += tx_bytes;
        }
    }
    fclose(file);
    
    // Calcular velocidad (KB/s)
    if (self->prev_rx_bytes > 0) {
        self->current_rx_speed = (total_rx - self->prev_rx_bytes) / 1024.0; // KB/s
        self->current_tx_speed = (total_tx - self->prev_tx_bytes) / 1024.0;
        
        // Actualizar escala máxima
        double max_current = (self->current_rx_speed > self->current_tx_speed) ? 
                            self->current_rx_speed : self->current_tx_speed;
        if (max_current > self->max_speed) {
            self->max_speed = max_current;
        }
    }
    
    self->prev_rx_bytes = total_rx;
    self->prev_tx_bytes = total_tx;
}

static void update_tooltip(NetMonitorWidget *self) {
    const gchar *rx_unit = "KB/s";
    const gchar *tx_unit = "KB/s";
    double rx_display = self->current_rx_speed;
    double tx_display = self->current_tx_speed;
    
    if (rx_display > 1024) {
        rx_display /= 1024.0;
        rx_unit = "MB/s";
    }
    if (tx_display > 1024) {
        tx_display /= 1024.0;
        tx_unit = "MB/s";
    }
    
    gchar *tooltip = g_strdup_printf(
        _("Network:\n"
        "↓ %.1f %s\n"
        "↑ %.1f %s"),
        rx_display, rx_unit,
        tx_display, tx_unit
    );
    
    gtk_widget_set_tooltip_text(GTK_WIDGET(self), tooltip);
    g_free(tooltip);
}

static void draw_network_graph(GtkDrawingArea *area G_GNUC_UNUSED, cairo_t *cr,
                              int width, int height, gpointer user_data) {
    NetMonitorWidget *self = NET_MONITOR_WIDGET(user_data);
    
    // Fondo
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.3);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    if (self->max_speed <= 0) return;
    
    double x_step = (double)width / (HISTORY_SIZE - 1);
    double scale = (double)height / self->max_speed;
    
    cairo_set_line_width(cr, 1.0);
    
    // Línea RX (descarga) - Verde
    cairo_set_source_rgba(cr, 0.0, 0.8, 0.0, 1.0);
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        int idx1 = (self->history_index + i) % HISTORY_SIZE;
        int idx2 = (self->history_index + i + 1) % HISTORY_SIZE;
        
        double x1 = i * x_step;
        double x2 = (i + 1) * x_step;
        double y1 = height - (self->rx_history[idx1] * scale);
        double y2 = height - (self->rx_history[idx2] * scale);
        
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }
    
    // Línea TX (subida) - Rojo
    cairo_set_source_rgba(cr, 0.8, 0.0, 0.0, 1.0);
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        int idx1 = (self->history_index + i) % HISTORY_SIZE;
        int idx2 = (self->history_index + i + 1) % HISTORY_SIZE;
        
        double x1 = i * x_step;
        double x2 = (i + 1) * x_step;
        double y1 = height - (self->tx_history[idx1] * scale);
        double y2 = height - (self->tx_history[idx2] * scale);
        
        cairo_move_to(cr, x1, y1);
        cairo_line_to(cr, x2, y2);
        cairo_stroke(cr);
    }
}

static gboolean on_update_network(gpointer user_data) {
    NetMonitorWidget *self = NET_MONITOR_WIDGET(user_data);
    
    read_network_info(self);
    
    // Actualizar historial
    self->rx_history[self->history_index] = self->current_rx_speed;
    self->tx_history[self->history_index] = self->current_tx_speed;
    self->history_index = (self->history_index + 1) % HISTORY_SIZE;
    
    update_tooltip(self);
    
    // Redibujar
    gtk_widget_queue_draw(self->drawing_area);
    
    return G_SOURCE_CONTINUE;
}

static void net_monitor_widget_dispose(GObject *object) {
    NetMonitorWidget *self = NET_MONITOR_WIDGET(object);
    
    if (self->timeout_id > 0) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
    
    G_OBJECT_CLASS(net_monitor_widget_parent_class)->dispose(object);
}

static void net_monitor_widget_init(NetMonitorWidget *self) {
    // Inicializar historial
    for (int i = 0; i < HISTORY_SIZE; i++) {
        self->rx_history[i] = 0.0;
        self->tx_history[i] = 0.0;
    }
    self->history_index = 0;
    self->max_speed = 100.0; // Escala inicial: 100 KB/s
    
    // Crear drawing area para la gráfica
    self->drawing_area = gtk_drawing_area_new();
    gtk_widget_add_css_class(self->drawing_area, "net-monitor-graph");
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(self->drawing_area),
                                   draw_network_graph, self, NULL);
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
    read_network_info(self);
    
    // Timer cada segundo
    self->timeout_id = g_timeout_add_seconds(1, on_update_network, self);
}

static void net_monitor_widget_class_init(NetMonitorWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = net_monitor_widget_dispose;
}

GtkWidget *net_monitor_widget_new(void) {
    return g_object_new(NET_TYPE_MONITOR_WIDGET, NULL);
}
