#include "panel.h"
#include "plugins/app_menu_button.h"
#include "plugins/launcher_widget.h"
#include "plugins/systray_widget.h"
#include "plugins/clock_widget.h"
#include "plugins/tasklist_widget.h"
#include "plugins/showdesktop_widget.h"
#include "config.h"
#include <gtk4-layer-shell.h> // Para el reloj

// Definición de la estructura interna de nuestro objeto PanelWindow
struct _PanelWindow {
    GtkApplicationWindow parent_instance;
    
    // Configuración
    PanelConfig *config;
    
    // Widgets del panel
    GtkBox *main_box;
    GtkWidget *app_menu_button;
    GtkWidget *launcher_widget;
    GtkWidget *tasklist_widget;
    GtkWidget *systray_widget;
    GtkWidget *clock_widget;
    GtkWidget *showdesktop_widget;
};

// Conecta la implementación con el sistema de tipos de GObject
G_DEFINE_TYPE(PanelWindow, panel_window, GTK_TYPE_APPLICATION_WINDOW)

// Aplicar configuración del reloj
static void panel_window_apply_clock_config(PanelWindow *self) {
    if (!self->clock_widget) return;
    
    // Crear CSS dinámico para el reloj
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gchar *css_data = g_strdup_printf(
        ".clock-button {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 0px;"
        "  padding: 4px 8px;"
        "  color: %s;"
        "}"
        ".clock-label {"
        "  font-size: %dpx;"
        "  font-weight: %s;"
        "  color: inherit;"
        "}",
        self->config->clock_color,
        self->config->clock_size,
        self->config->clock_weight
    );
    
    gtk_css_provider_load_from_string(css_provider, css_data);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER
    );
    
    g_free(css_data);
    g_object_unref(css_provider);
}

// Función de limpieza
static void panel_window_dispose(GObject *object) {
    PanelWindow *self = PANEL_WINDOW(object);
    
    if (self->config) {
        panel_config_free(self->config);
        self->config = NULL;
    }
    
    G_OBJECT_CLASS(panel_window_parent_class)->dispose(object);
}

// Función de inicialización para una instancia de PanelWindow
static void panel_window_init(PanelWindow *self) {
    // 1. Cargar configuración
    self->config = panel_config_new();
    gchar *config_path = panel_config_get_default_path();
    
    // Si no existe archivo de configuración, crear uno por defecto
    if (!g_file_test(config_path, G_FILE_TEST_EXISTS)) {
        panel_config_create_default(config_path);
    }
    
    // Cargar configuración
    if (!panel_config_load(self->config, config_path)) {
        g_warning("Usando configuración por defecto");
    }
    
    g_free(config_path);

    // 2. Inicializar la superficie de capa (layer surface)
    gtk_layer_init_for_window(GTK_WINDOW(self));

    // 3. Configurar posición del panel según configuración
    if (g_strcmp0(self->config->edge, "top") == 0) {
        gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
    } else {
        gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
    }
    gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
    gtk_layer_set_anchor(GTK_WINDOW(self), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

    // 4. Establecer altura del panel según configuración
    gtk_widget_set_size_request(GTK_WIDGET(self), -1, self->config->panel_size);
    gtk_layer_set_layer(GTK_WINDOW(self), GTK_LAYER_SHELL_LAYER_TOP);
    gtk_layer_auto_exclusive_zone_enable(GTK_WINDOW(self));

    // 5. Contenedor principal (un GtkBox horizontal)
    self->main_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));
    gtk_window_set_child(GTK_WINDOW(self), GTK_WIDGET(self->main_box));

    // --- Plugins configurables ---

    // Plugin: Menú principal (a la izquierda) - solo si está habilitado
    if (self->config->menu_enable) {
        self->app_menu_button = app_menu_button_new(self->config->menu_icon, self->config);
        gtk_box_append(self->main_box, self->app_menu_button);
    }

    // Plugin: Launchers - botones de lanzamiento rápido
    self->launcher_widget = launcher_widget_new(self->config);
    gtk_box_append(self->main_box, self->launcher_widget);

    // Plugin: Lista de Tareas (centro) - siempre habilitado, toma el espacio restante
    self->tasklist_widget = GTK_WIDGET(tasklist_widget_new(self->config));
    gtk_widget_set_hexpand(self->tasklist_widget, TRUE); // Para que ocupe el espacio sobrante
    gtk_box_append(self->main_box, self->tasklist_widget);
    
    // Plugin: Área de Notificación (System Tray) - solo si está habilitado
    if (self->config->systray_enable) {
        self->systray_widget = systray_widget_new(self->config);
        gtk_box_append(self->main_box, self->systray_widget);
    }
    
    // Plugin: Reloj (a la derecha) - solo si está habilitado
    if (self->config->clock_enable) {
        self->clock_widget = clock_widget_new();
        
        // Aplicar configuración del reloj
        panel_window_apply_clock_config(self);
        
        gtk_box_append(self->main_box, self->clock_widget);
    }

    // Plugin: Show Desktop - solo si está habilitado
    if (self->config->showdesktop_enable) {
        self->showdesktop_widget = GTK_WIDGET(showdesktop_widget_new(self->config));
        gtk_box_append(self->main_box, self->showdesktop_widget);
    }
}

// Función de inicialización de la clase PanelWindow
static void panel_window_class_init(PanelWindowClass * G_GNUC_UNUSED klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = panel_window_dispose;
}

// Función "constructora" pública
GtkWidget *panel_window_new(GtkApplication *app) {
    return g_object_new(PANEL_TYPE_WINDOW, "application", app, NULL);
}
