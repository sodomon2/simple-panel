#include "clock_widget.h"
#include "../i18n.h"
#include <time.h>

struct _ClockWidget {
    GtkBox parent_instance;
    GtkWidget *clock_button;
    GtkWidget *clock_label;
    GtkWidget *calendar_popover;
    guint timeout_id;
};

G_DEFINE_TYPE(ClockWidget, clock_widget, GTK_TYPE_BOX)

static void update_clock_display(ClockWidget *self) {
    time_t rawtime;
    struct tm *timeinfo;
    char time_buffer[80];
    char date_buffer[80];
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    // Formato para el label: solo HH:MM
    strftime(time_buffer, sizeof(time_buffer), "%H:%M", timeinfo);
    gtk_label_set_text(GTK_LABEL(self->clock_label), time_buffer);
    
    // Formato para el tooltip: fecha completa
    strftime(date_buffer, sizeof(date_buffer), "%A, %d %B %Y", timeinfo);
    gtk_widget_set_tooltip_text(self->clock_button, date_buffer);
}

static gboolean on_clock_timeout(gpointer user_data) {
    ClockWidget *self = CLOCK_WIDGET(user_data);
    update_clock_display(self);
    return G_SOURCE_CONTINUE; // Continuar el timeout
}

static void on_clock_button_clicked(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    ClockWidget *self = CLOCK_WIDGET(user_data);
    
    if (gtk_widget_get_visible(self->calendar_popover)) {
        gtk_popover_popdown(GTK_POPOVER(self->calendar_popover));
    } else {
        gtk_popover_popup(GTK_POPOVER(self->calendar_popover));
    }
}

static void clock_widget_init(ClockWidget *self) {
    // Crear botón que contendrá el reloj
    self->clock_button = gtk_button_new();
    gtk_widget_set_tooltip_text(self->clock_button, _("Show calendar"));
    gtk_box_append(GTK_BOX(self), self->clock_button);
    
    // Crear etiqueta del reloj
    self->clock_label = gtk_label_new("00:00");
    gtk_button_set_child(GTK_BUTTON(self->clock_button), self->clock_label);
    
    // Crear popover del calendario
    self->calendar_popover = gtk_popover_new();
    gtk_widget_set_parent(self->calendar_popover, self->clock_button);
    gtk_popover_set_position(GTK_POPOVER(self->calendar_popover), GTK_POS_TOP);
    gtk_popover_set_autohide(GTK_POPOVER(self->calendar_popover), TRUE);
    gtk_popover_set_has_arrow(GTK_POPOVER(self->calendar_popover), FALSE);
    
    // Crear el widget calendario
    GtkWidget *calendar = gtk_calendar_new();
    gtk_popover_set_child(GTK_POPOVER(self->calendar_popover), calendar);
    
    // Cargar CSS desde GResource
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/io/gitlab/sodomon/simple_panel/styles/clock-styles.css");
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // Aplicar clases CSS
    gtk_widget_add_css_class(self->clock_button, "clock-button");
    gtk_widget_add_css_class(self->clock_label, "clock-label");
    
    // Conectar señales
    g_signal_connect(self->clock_button, "clicked", G_CALLBACK(on_clock_button_clicked), self);
    
    // Actualizar reloj inmediatamente
    update_clock_display(self);
    
    // Iniciar timer para actualizar cada segundo
    self->timeout_id = g_timeout_add_seconds(1, on_clock_timeout, self);
    
    // Limpiar CSS provider
    g_object_unref(css_provider);
}

static void clock_widget_dispose(GObject *object) {
    ClockWidget *self = CLOCK_WIDGET(object);
    
    // Detener el timeout
    if (self->timeout_id > 0) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
    
    // Limpiar popover
    if (self->calendar_popover) {
        gtk_widget_unparent(self->calendar_popover);
        self->calendar_popover = NULL;
    }
    
    G_OBJECT_CLASS(clock_widget_parent_class)->dispose(object);
}

static void clock_widget_class_init(ClockWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = clock_widget_dispose;
}

GtkWidget *clock_widget_new(void) {
    return g_object_new(CLOCK_TYPE_WIDGET, NULL);
}
