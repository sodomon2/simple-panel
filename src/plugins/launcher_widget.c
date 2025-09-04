#include "launcher_widget.h"
#include <stdlib.h>

typedef struct {
    gchar *name;
    gboolean enable;
    gchar *icon;
    gchar *tooltip;
    gchar *command;
    gchar *type;
    gint order;
} LauncherItem;

struct _LauncherWidget {
    GtkBox parent_instance;
    PanelConfig *config;
    GSList *launcher_items;
};

G_DEFINE_TYPE(LauncherWidget, launcher_widget, GTK_TYPE_BOX)

// Liberar memoria de un launcher item
static void launcher_item_free(LauncherItem *item) {
    if (!item) return;
    g_free(item->name);
    g_free(item->icon);
    g_free(item->tooltip);
    g_free(item->command);
    g_free(item->type);
    g_free(item);
}

// Callback para ejecutar comando del launcher
static void on_launcher_clicked(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    LauncherItem *item = (LauncherItem *)user_data;
    
    if (!item->command) return;
    
    // Ejecutar comando en background
    GError *error = NULL;
    gchar **argv = NULL;
    
    if (g_shell_parse_argv(item->command, NULL, &argv, &error)) {
        g_spawn_async(NULL, argv, NULL, 
                     G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                     NULL, NULL, NULL, &error);
        g_strfreev(argv);
    }
    
    if (error) {
        g_warning("Error ejecutando launcher '%s': %s", item->name, error->message);
        g_error_free(error);
    }
}

// Función de comparación para ordenar launcher items
static gint launcher_item_compare(gconstpointer a, gconstpointer b) {
    const LauncherItem *item_a = (const LauncherItem *)a;
    const LauncherItem *item_b = (const LauncherItem *)b;
    return item_a->order - item_b->order;
}

// Cargar launcher items desde configuración
static void load_launcher_items(LauncherWidget *self) {
    gchar *config_path = panel_config_get_default_path();
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    
    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, &error)) {
        g_warning("No se pudo cargar configuración para launchers: %s", error->message);
        g_error_free(error);
        g_key_file_free(key_file);
        g_free(config_path);
        return;
    }
    
    // Verificar si los launchers están habilitados globalmente
    if (!g_key_file_has_group(key_file, "launchers") || 
        !g_key_file_get_boolean(key_file, "launchers", "enable", NULL)) {
        g_key_file_free(key_file);
        g_free(config_path);
        return;
    }
    
    // Obtener lista de items a cargar
    gchar *items_string = g_key_file_get_string(key_file, "launchers", "items", NULL);
    if (!items_string) {
        g_key_file_free(key_file);
        g_free(config_path);
        return;
    }
    
    gchar **items = g_strsplit(items_string, ",", -1);
    g_free(items_string);
    
    // Cargar cada launcher item
    for (gint i = 0; items[i] != NULL; i++) {
        gchar *item_name = g_strstrip(items[i]);
        gchar *section_name = g_strdup_printf("launcher:%s", item_name);
        
        if (g_key_file_has_group(key_file, section_name)) {
            LauncherItem *item = g_malloc0(sizeof(LauncherItem));
            item->name = g_strdup(item_name);
            
            // Cargar propiedades del item
            item->enable = g_key_file_get_boolean(key_file, section_name, "enable", NULL);
            item->icon = g_key_file_get_string(key_file, section_name, "icon", NULL);
            item->tooltip = g_key_file_get_string(key_file, section_name, "tooltip", NULL);
            item->command = g_key_file_get_string(key_file, section_name, "command", NULL);
            item->type = g_key_file_get_string(key_file, section_name, "type", NULL);
            item->order = g_key_file_get_integer(key_file, section_name, "order", NULL);
            
            // Solo agregar si está habilitado (excepto separadores)
            if (item->enable || (item->type && g_strcmp0(item->type, "separator") == 0)) {
                self->launcher_items = g_slist_append(self->launcher_items, item);
            } else {
                launcher_item_free(item);
            }
        }
        
        g_free(section_name);
    }
    
    g_strfreev(items);
    g_key_file_free(key_file);
    g_free(config_path);
    
    // Ordenar items por orden especificado
    self->launcher_items = g_slist_sort(self->launcher_items, 
        (GCompareFunc)launcher_item_compare);
}

// Crear widgets para los launchers
static void create_launcher_widgets(LauncherWidget *self) {
    // Aplicar CSS para el estilo de los launchers
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider,
        ".launcher-button {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 0px;"
        "  padding: 4px 6px;"
        "  margin: 0px 2px;"
        "  min-width: 24px;"
        "  min-height: 24px;"
        "}"
        ".launcher-button:hover {"
        "  background: rgba(255, 255, 255, 0.1);"
        "}"
        ".launcher-button:active {"
        "  background: rgba(255, 255, 255, 0.2);"
        "}"
        ".launcher-separator {"
        "  background: rgba(255, 255, 255, 0.3);"
        "  min-width: 1px;"
        "  min-height: 20px;"
        "  margin: 6px 4px;"
        "}"
    );
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // Liberar CSS provider inmediatamente después de usarlo
    g_object_unref(css_provider);
    
    // Crear widget para cada launcher item
    for (GSList *l = self->launcher_items; l != NULL; l = l->next) {
        LauncherItem *item = (LauncherItem *)l->data;
        
        if (item->type && g_strcmp0(item->type, "separator") == 0) {
            // Crear separador
            GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
            gtk_widget_add_css_class(separator, "launcher-separator");
            gtk_box_append(GTK_BOX(self), separator);
        } else {
            // Crear botón launcher
            GtkWidget *button = gtk_button_new();
            gtk_widget_add_css_class(button, "launcher-button");
            
            // Configurar icono
            if (item->icon) {
                gtk_button_set_icon_name(GTK_BUTTON(button), item->icon);
            }
            
            // Configurar tooltip
            if (item->tooltip) {
                gtk_widget_set_tooltip_text(button, item->tooltip);
            }
            
            // Conectar callback
            g_signal_connect(button, "clicked", G_CALLBACK(on_launcher_clicked), item);
            
            gtk_box_append(GTK_BOX(self), button);
        }
    }
}

static void launcher_widget_init(LauncherWidget *self) {
    self->launcher_items = NULL;
    
    // Cargar configuración y crear widgets
    load_launcher_items(self);
    create_launcher_widgets(self);
}

static void launcher_widget_dispose(GObject *object) {
    LauncherWidget *self = LAUNCHER_WIDGET(object);
    
    // Limpiar lista de launcher items
    for (GSList *l = self->launcher_items; l != NULL; l = l->next) {
        launcher_item_free((LauncherItem *)l->data);
    }
    g_slist_free(self->launcher_items);
    self->launcher_items = NULL;
    
    G_OBJECT_CLASS(launcher_widget_parent_class)->dispose(object);
}

static void launcher_widget_class_init(LauncherWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = launcher_widget_dispose;
}

GtkWidget *launcher_widget_new(PanelConfig *config) {
    LauncherWidget *self = g_object_new(LAUNCHER_TYPE_WIDGET, NULL);
    self->config = config;
    return GTK_WIDGET(self);
}
