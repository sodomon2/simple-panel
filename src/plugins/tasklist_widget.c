#define _GNU_SOURCE
#include "tasklist_widget.h"
#include <wayland-client.h>

#ifdef HAVE_WLR_PROTOCOLS
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#endif

// Estructura para cada ventana en la tasklist
typedef struct {
    gchar *app_id;
    gchar *title;
    GtkWidget *button;
    gboolean is_active;
    gboolean is_minimized;
    
#ifdef HAVE_WLR_PROTOCOLS
    struct zwlr_foreign_toplevel_handle_v1 *toplevel_handle;
#endif
    
    TasklistWidget *tasklist;
} TaskItem;

// Estructura principal del widget
struct _TasklistWidget {
    GtkBox parent_instance;
    
    // Configuración
    PanelConfig *config;
    
    // Wayland connection
#ifdef HAVE_WLR_PROTOCOLS
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_seat *wl_seat;
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager;
#endif
    
    // Task management
    GSList *task_items;         // Lista de TaskItems
    TaskItem *active_task;      // Ventana actualmente activa
};

G_DEFINE_TYPE(TasklistWidget, tasklist_widget, GTK_TYPE_BOX)

#ifdef HAVE_WLR_PROTOCOLS
// Forward declarations
static void setup_wayland_connection(TasklistWidget *self);
static void setup_wayland_event_integration(TasklistWidget *self);
static gboolean wayland_event_callback(GIOChannel *channel, GIOCondition condition, gpointer user_data);
static TaskItem *create_task_item(TasklistWidget *tasklist, const char *app_id, const char *title);
static void remove_task_item(TasklistWidget *tasklist, TaskItem *item);
static void update_task_button_state(TaskItem *item);
static gchar *get_icon_name_for_app_id(const gchar *app_id);
static void update_task_button_icon(TaskItem *item);

// Liberar memoria de un task item
static void task_item_free(TaskItem *item) {
    if (!item) return;
    
    g_free(item->app_id);
    g_free(item->title);
    
    if (item->button) {
        gtk_widget_unparent(item->button);
    }
    
    if (item->toplevel_handle) {
        zwlr_foreign_toplevel_handle_v1_destroy(item->toplevel_handle);
    }
    
    g_free(item);
}

// Callback cuando se hace clic en un botón de tarea
static void on_task_button_clicked(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    TaskItem *item = (TaskItem *)user_data;
    
    if (!item || !item->toplevel_handle || !item->tasklist) {
        return;
    }
    
    if (!item->tasklist->wl_seat) {
        return;
    }
    
    if (!item->tasklist->wl_display) {
        return;
    }
    
    // Comportamiento inteligente según el estado actual
    if (item->is_active && !item->is_minimized) {
        // Si está activa y no minimizada -> minimizar
        zwlr_foreign_toplevel_handle_v1_set_minimized(item->toplevel_handle);
    } else {
        // Si está minimizada o no activa -> activar
        zwlr_foreign_toplevel_handle_v1_activate(item->toplevel_handle, item->tasklist->wl_seat);
    }
    
    // ¡CRÍTICO! Forzar envío inmediato de comandos al compositor
    wl_display_flush(item->tasklist->wl_display);
    
    // Procesar respuesta del compositor
    wl_display_roundtrip(item->tasklist->wl_display);
}

// Crear botón GTK para una tarea
static GtkWidget *create_task_button(TaskItem *item) {
    GtkWidget *button = gtk_button_new();
    gtk_widget_add_css_class(button, "task-button");
    
    // Layout horizontal: icono + texto
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_button_set_child(GTK_BUTTON(button), box);
    
    // Icono - intentar obtener el correcto desde el principio
    gchar *icon_name = get_icon_name_for_app_id(item->app_id);
    GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
    gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_NORMAL);
    gtk_box_append(GTK_BOX(box), icon);
    g_free(icon_name);
    
    // Texto del título
    const gchar *display_text = item->title ? item->title : 
                               (item->app_id ? item->app_id : "Aplicación");
    GtkWidget *label = gtk_label_new(display_text);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(label), 15);
    gtk_widget_set_hexpand(label, FALSE);
    gtk_box_append(GTK_BOX(box), label);
    
    // Conectar señal de clic
    g_signal_connect(button, "clicked", G_CALLBACK(on_task_button_clicked), item);
    
    return button;
}

// Crear nuevo item de tarea
static TaskItem *create_task_item(TasklistWidget *tasklist, const char *app_id, const char *title) {
    TaskItem *item = g_malloc0(sizeof(TaskItem));
    item->tasklist = tasklist;
    item->app_id = app_id ? g_strdup(app_id) : NULL;
    item->title = title ? g_strdup(title) : NULL;
    item->is_active = FALSE;
    item->is_minimized = FALSE;
    
    // Crear botón GTK
    item->button = create_task_button(item);
    
    // Añadir al contenedor
    gtk_box_append(GTK_BOX(tasklist), item->button);
    
    // Añadir a la lista
    tasklist->task_items = g_slist_append(tasklist->task_items, item);

    
    return item;
}

// Remover item de tarea
static void remove_task_item(TasklistWidget *tasklist, TaskItem *item) {
    if (!item) return;
    
    // Remover de la lista
    tasklist->task_items = g_slist_remove(tasklist->task_items, item);
    
    // Si era la ventana activa, limpiar referencia
    if (tasklist->active_task == item) {
        tasklist->active_task = NULL;
    }
    
    // Liberar memoria
    task_item_free(item);
}

// Actualizar estado visual del botón
static void update_task_button_state(TaskItem *item) {
    if (!item->button) return;
    
    // Remover clases existentes
    gtk_widget_remove_css_class(item->button, "active");
    gtk_widget_remove_css_class(item->button, "minimized");
    
    // Aplicar clases según estado
    if (item->is_active) {
        gtk_widget_add_css_class(item->button, "active");
        item->tasklist->active_task = item;
    }
    
    if (item->is_minimized) {
        gtk_widget_add_css_class(item->button, "minimized");
    }
}

// Buscar TaskItem por toplevel handle
static TaskItem *find_task_by_toplevel(TasklistWidget *tasklist, 
                                      struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    for (GSList *l = tasklist->task_items; l != NULL; l = l->next) {
        TaskItem *item = (TaskItem *)l->data;
        if (item->toplevel_handle == toplevel) {
            return item;
        }
    }
    return NULL;
}

// === SISTEMA DE ICONOS DINÁMICO ===

// Buscar archivo .desktop correspondiente al app_id
static gchar *find_desktop_file(const gchar *app_id) {
    if (!app_id) return NULL;
    
    // Directorios comunes para archivos .desktop
    const gchar *desktop_dirs[] = {
        "/usr/share/applications/",
        "/usr/local/share/applications/",
        "~/.local/share/applications/",
        NULL
    };
    
    // Patrones de nombre comunes
    gchar *patterns[] = {
        g_strdup_printf("%s.desktop", app_id),
        g_strdup_printf("org.%s.desktop", app_id),
        g_strdup_printf("com.%s.desktop", app_id),
        NULL
    };
    
    for (int d = 0; desktop_dirs[d]; d++) {
        for (int p = 0; patterns[p]; p++) {
            gchar *full_path = g_build_filename(desktop_dirs[d], patterns[p], NULL);
            
            if (g_file_test(full_path, G_FILE_TEST_EXISTS)) {
                // Liberar patrones
                for (int i = 0; patterns[i]; i++) g_free(patterns[i]);
                return full_path;
            }
            
            g_free(full_path);
        }
    }
    
    // Liberar patrones
    for (int i = 0; patterns[i]; i++) g_free(patterns[i]);
    return NULL;
}

// Extraer nombre de icono de archivo .desktop
static gchar *extract_icon_from_desktop_file(const gchar *desktop_path) {
    GKeyFile *key_file = g_key_file_new();
    GError *error = NULL;
    gchar *icon_name = NULL;
    
    if (g_key_file_load_from_file(key_file, desktop_path, G_KEY_FILE_NONE, &error)) {
        icon_name = g_key_file_get_string(key_file, "Desktop Entry", "Icon", NULL);
    } else {
        g_debug("Error leyendo %s: %s", desktop_path, error->message);
        g_error_free(error);
    }
    
    g_key_file_free(key_file);
    return icon_name;
}

// Función principal para obtener nombre de icono - 100% dinámico
static gchar *get_icon_name_for_app_id(const gchar *app_id) {
    if (!app_id) return g_strdup("application-x-executable");
    
    // 1. Buscar archivo .desktop de la aplicación
    gchar *desktop_file = find_desktop_file(app_id);
    if (desktop_file) {
        gchar *icon_name = extract_icon_from_desktop_file(desktop_file);
        g_free(desktop_file);
        
        if (icon_name && strlen(icon_name) > 0) {
            return icon_name;
        }
        g_free(icon_name);
    }
    
    // 2. Usar app_id directamente (muchas apps usan su app_id como nombre de icono)
    return g_strdup(app_id);
}

// Actualizar icono del botón de tarea
static void update_task_button_icon(TaskItem *item) {
    if (!item || !item->button) return;
    
    // Obtener nombre de icono
    gchar *icon_name = get_icon_name_for_app_id(item->app_id);
    
    // Encontrar el widget de icono en el botón
    GtkWidget *button_child = gtk_button_get_child(GTK_BUTTON(item->button));
    if (GTK_IS_BOX(button_child)) {
        GtkWidget *icon_widget = gtk_widget_get_first_child(button_child);
        if (GTK_IS_IMAGE(icon_widget)) {
            // Verificar si el icono existe en el tema
            GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
            
            if (gtk_icon_theme_has_icon(icon_theme, icon_name)) {
                gtk_image_set_from_icon_name(GTK_IMAGE(icon_widget), icon_name);
            } else {
                // Fallback a icono genérico
                gtk_image_set_from_icon_name(GTK_IMAGE(icon_widget), "application-x-executable");
            }
        }
    }
    
    g_free(icon_name);
}

// === CALLBACKS DEL PROTOCOLO WAYLAND ===

// Callback: título actualizado
static void toplevel_handle_title(void *data, 
                                 struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                 const char *title) {
    TasklistWidget *tasklist = TASKLIST_WIDGET(data);
    TaskItem *item = find_task_by_toplevel(tasklist, toplevel);
    
    if (item) {
        g_free(item->title);
        item->title = g_strdup(title);
        
        // Actualizar texto del botón
        GtkWidget *box = gtk_button_get_child(GTK_BUTTON(item->button));
        GtkWidget *label = gtk_widget_get_last_child(box);
        if (GTK_IS_LABEL(label)) {
            gtk_label_set_text(GTK_LABEL(label), title);
        }
    }
}

// Callback: app_id actualizado
static void toplevel_handle_app_id(void *data,
                                  struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                  const char *app_id) {
    TasklistWidget *tasklist = TASKLIST_WIDGET(data);
    TaskItem *item = find_task_by_toplevel(tasklist, toplevel);
    
    if (item) {
        g_free(item->app_id);
        item->app_id = g_strdup(app_id);
        
        // Actualizar icono cuando recibimos el app_id
        update_task_button_icon(item);
    }
}

// Callback: estado de ventana cambiado
static void toplevel_handle_state(void *data,
                                 struct zwlr_foreign_toplevel_handle_v1 *toplevel,
                                 struct wl_array *state) {
    TasklistWidget *tasklist = TASKLIST_WIDGET(data);
    TaskItem *item = find_task_by_toplevel(tasklist, toplevel);
    
    if (!item) return;
    
    // Resetear estados
    item->is_active = FALSE;
    item->is_minimized = FALSE;
    
    // Procesar nuevos estados
    uint32_t *entry;
    wl_array_for_each(entry, state) {
        switch (*entry) {
        case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
            item->is_active = TRUE;
            break;
        case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
            item->is_minimized = TRUE;
            break;
        }
    }
    
    // Actualizar UI
    update_task_button_state(item);
}

// Callback: configuración completada
static void toplevel_handle_done(void *data G_GNUC_UNUSED,
                                struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED) {
    // Configuración de ventana completada, no necesitamos hacer nada especial
}

// Callback: ventana cerrada
static void toplevel_handle_closed(void *data,
                                  struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    TasklistWidget *tasklist = TASKLIST_WIDGET(data);
    TaskItem *item = find_task_by_toplevel(tasklist, toplevel);
    
    if (item) {
        remove_task_item(tasklist, item);
    }
}

// Callbacks no críticos
static void toplevel_handle_output_enter(void *data G_GNUC_UNUSED, struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, struct wl_output *output G_GNUC_UNUSED) { }
static void toplevel_handle_output_leave(void *data G_GNUC_UNUSED, struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, struct wl_output *output G_GNUC_UNUSED) { }
static void toplevel_handle_parent(void *data G_GNUC_UNUSED, struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, struct zwlr_foreign_toplevel_handle_v1 *parent G_GNUC_UNUSED) { }

// Listener para ventanas individuales
static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
    .title = toplevel_handle_title,
    .app_id = toplevel_handle_app_id,
    .output_enter = toplevel_handle_output_enter,
    .output_leave = toplevel_handle_output_leave,
    .state = toplevel_handle_state,
    .done = toplevel_handle_done,
    .closed = toplevel_handle_closed,
    .parent = toplevel_handle_parent,
};

// Callback cuando aparece una nueva ventana
static void toplevel_manager_handle_toplevel(void *data,
                                           struct zwlr_foreign_toplevel_manager_v1 *manager G_GNUC_UNUSED,
                                           struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    TasklistWidget *tasklist = TASKLIST_WIDGET(data);
    
    // Crear item temporal (título y app_id llegarán después)
    TaskItem *item = create_task_item(tasklist, NULL, "Cargando...");
    item->toplevel_handle = toplevel;
    
    // Añadir listener a esta ventana
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &toplevel_handle_listener, tasklist);
    

}

// Callback: manager terminado
static void toplevel_manager_handle_finished(void *data G_GNUC_UNUSED,
                                           struct zwlr_foreign_toplevel_manager_v1 *manager G_GNUC_UNUSED) {
}

// Listener para el manager principal
static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_listener = {
    .toplevel = toplevel_manager_handle_toplevel,
    .finished = toplevel_manager_handle_finished,
};

// Callback del registry
static void registry_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface, uint32_t version) {
    TasklistWidget *tasklist = TASKLIST_WIDGET(data);
    

    
    if (strcmp(interface, wl_seat_interface.name) == 0) {
        tasklist->wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, 
                                           version < 7 ? version : 7);
        if (tasklist->wl_seat) {

        } else {

        }
    }
    else if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        tasklist->toplevel_manager = wl_registry_bind(registry, name, 
                                                     &zwlr_foreign_toplevel_manager_v1_interface, 
                                                     version);
        
        if (tasklist->toplevel_manager) {
            zwlr_foreign_toplevel_manager_v1_add_listener(tasklist->toplevel_manager, 
                                                         &toplevel_manager_listener, 
                                                         tasklist);

        } else {

        }
    }
}

static void registry_global_remove(void *data G_GNUC_UNUSED, struct wl_registry *registry G_GNUC_UNUSED, uint32_t name G_GNUC_UNUSED) {
    // Manejar remoción de interfaces si es necesario
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

// Configurar conexión Wayland
static void setup_wayland_connection(TasklistWidget *self) {
    if (!self) {
        g_critical("TasklistWidget: self es NULL en setup_wayland_connection");
        return;
    }
    

    
    // Conectar a Wayland display
    self->wl_display = wl_display_connect(NULL);
    if (!self->wl_display) {

        return;
    }
    

    
    // Obtener registry
    self->wl_registry = wl_display_get_registry(self->wl_display);
    if (!self->wl_registry) {

        wl_display_disconnect(self->wl_display);
        self->wl_display = NULL;
        return;
    }
    
    // Configurar listener del registry
    if (wl_registry_add_listener(self->wl_registry, &registry_listener, self) == -1) {

        wl_registry_destroy(self->wl_registry);
        wl_display_disconnect(self->wl_display);
        self->wl_registry = NULL;
        self->wl_display = NULL;
        return;
    }
    
    // Procesar eventos iniciales de forma segura
    if (wl_display_dispatch(self->wl_display) == -1) {

    }
    wl_display_dispatch(self->wl_display);
    
    if (!self->toplevel_manager) {

        wl_registry_destroy(self->wl_registry);
        return;
    }
    

    
    // Configurar integración automática de eventos (toplevel_manager ya verificado)
    setup_wayland_event_integration(self);
}

// Callback para procesar eventos de Wayland desde GTK main loop  
static gboolean wayland_event_callback(GIOChannel *channel G_GNUC_UNUSED, GIOCondition condition, gpointer user_data) {
    TasklistWidget *self = TASKLIST_WIDGET(user_data);
    
    if (condition & G_IO_IN) {
        // Procesar eventos pendientes sin bloquear
        if (wl_display_dispatch(self->wl_display) == -1) {

            return G_SOURCE_REMOVE;
        }
    }
    
    if (condition & (G_IO_ERR | G_IO_HUP)) {

        return G_SOURCE_REMOVE;
    }
    
    return G_SOURCE_CONTINUE;
}

// Configurar integración de eventos Wayland con GTK main loop
static void setup_wayland_event_integration(TasklistWidget *self) {
    if (!self->wl_display) {

        return;
    }
    
    // Obtener file descriptor de Wayland
    int wayland_fd = wl_display_get_fd(self->wl_display);
    if (wayland_fd < 0) {

        return;
    }
    
    // Crear canal GIO para el fd
    GIOChannel *wayland_channel = g_io_channel_unix_new(wayland_fd);
    if (!wayland_channel) {

        return;
    }
    
    // Configurar canal para no bloquear
    g_io_channel_set_encoding(wayland_channel, NULL, NULL);
    g_io_channel_set_buffered(wayland_channel, FALSE);
    g_io_channel_set_close_on_unref(wayland_channel, FALSE);
    
    // Añadir watch al main loop de GTK para eventos Wayland
    guint watch_id = g_io_add_watch(wayland_channel,
                                   G_IO_IN | G_IO_ERR | G_IO_HUP,
                                   wayland_event_callback,
                                   self);
    
    // Guardar para cleanup después (si añadimos dispose)
    g_object_set_data(G_OBJECT(self), "wayland-watch-id", GUINT_TO_POINTER(watch_id));
    
    g_io_channel_unref(wayland_channel);
    

}
#endif

// Aplicar estilos CSS
static void apply_tasklist_styles(void) {
    static gboolean styles_applied = FALSE;
    if (styles_applied) return;
    
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/io/gitlab/sodomon/simple_panel/styles/tasklist-styles.css");
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    g_object_unref(css_provider);
    styles_applied = TRUE;
}

// Inicialización de la clase
static void tasklist_widget_class_init(TasklistWidgetClass *class G_GNUC_UNUSED) {
    // Configuración de la clase (destructor, etc.)
}

// Inicialización de la instancia  
static void tasklist_widget_init(TasklistWidget *self) {
    // Verificación de seguridad
    if (!self) {
        g_critical("TasklistWidget: self es NULL en init");
        return;
    }
    
    // Configurar como box horizontal
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_HORIZONTAL);
    gtk_box_set_spacing(GTK_BOX(self), 4);
    gtk_widget_add_css_class(GTK_WIDGET(self), "tasklist");
    
    // Aplicar estilos
    apply_tasklist_styles();
    
    // Inicializar listas y conexión Wayland como NULL
    self->task_items = NULL;
    self->active_task = NULL;
    
#ifdef HAVE_WLR_PROTOCOLS
    self->wl_display = NULL;
    self->wl_registry = NULL;
    self->wl_seat = NULL;
    self->toplevel_manager = NULL;
    
    // Configurar conexión Wayland (puede fallar sin causar crash)
    setup_wayland_connection(self);
#else

#endif
}

// Constructor público
TasklistWidget *tasklist_widget_new(PanelConfig *config) {
    TasklistWidget *widget = g_object_new(TASKLIST_TYPE_WIDGET, NULL);
    widget->config = config;
    return widget;
}
