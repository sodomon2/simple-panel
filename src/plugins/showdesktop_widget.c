#include "showdesktop_widget.h"
#include <gtk/gtk.h>

#ifdef HAVE_WLR_PROTOCOLS
#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"
#endif

// Estructura del widget Show Desktop
struct _ShowDesktopWidget {
    GtkBox parent_instance;
    
    PanelConfig *config;
    GtkWidget *button;
    gboolean desktop_shown;
    
#ifdef HAVE_WLR_PROTOCOLS
    struct wl_display *wl_display;
    struct wl_registry *wl_registry;
    struct wl_seat *wl_seat;
    struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager;
    GSList *toplevel_handles;
#endif
};

G_DEFINE_TYPE(ShowDesktopWidget, showdesktop_widget, GTK_TYPE_BOX)

#ifdef HAVE_WLR_PROTOCOLS

// Forward declarations
static void setup_wayland_connection(ShowDesktopWidget *self);
static void setup_wayland_event_integration(ShowDesktopWidget *self);
static gboolean wayland_event_callback(GIOChannel *channel, GIOCondition condition, gpointer user_data);
static void minimize_all_windows(ShowDesktopWidget *self);
static void restore_windows(ShowDesktopWidget *self);

// Estructura para almacenar handles de ventanas
typedef struct {
    struct zwlr_foreign_toplevel_handle_v1 *handle;
    gboolean was_minimized;
} ToplevelHandle;

// Callback cuando se cierra una ventana
static void toplevel_handle_closed(void *data,
                                  struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    ShowDesktopWidget *self = SHOWDESKTOP_WIDGET(data);
    
    // Buscar y remover el handle de la lista
    GSList *current = self->toplevel_handles;
    while (current) {
        ToplevelHandle *handle_data = current->data;
        if (handle_data->handle == toplevel) {
            self->toplevel_handles = g_slist_remove(self->toplevel_handles, handle_data);
            zwlr_foreign_toplevel_handle_v1_destroy(toplevel);
            g_free(handle_data);
            break;
        }
        current = current->next;
    }
}

// Callbacks vacíos para el protocolo
static void toplevel_handle_title(void *data G_GNUC_UNUSED, 
                                 struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, 
                                 const char *title G_GNUC_UNUSED) { }
static void toplevel_handle_app_id(void *data G_GNUC_UNUSED, 
                                  struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, 
                                  const char *app_id G_GNUC_UNUSED) { }
static void toplevel_handle_output_enter(void *data G_GNUC_UNUSED, 
                                        struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, 
                                        struct wl_output *output G_GNUC_UNUSED) { }
static void toplevel_handle_output_leave(void *data G_GNUC_UNUSED, 
                                        struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, 
                                        struct wl_output *output G_GNUC_UNUSED) { }
static void toplevel_handle_state(void *data, 
                                 struct zwlr_foreign_toplevel_handle_v1 *toplevel, 
                                 struct wl_array *state) {
    ShowDesktopWidget *self = SHOWDESKTOP_WIDGET(data);
    
    // Buscar este handle y actualizar su estado
    GSList *current = self->toplevel_handles;
    while (current) {
        ToplevelHandle *handle_data = current->data;
        if (handle_data->handle == toplevel) {
            // Verificar si está minimizada
            uint32_t *s;
            gboolean is_minimized = FALSE;
            wl_array_for_each(s, state) {
                if (*s == ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED) {
                    is_minimized = TRUE;
                    break;
                }
            }
            handle_data->was_minimized = is_minimized;
            break;
        }
        current = current->next;
    }
}
static void toplevel_handle_done(void *data G_GNUC_UNUSED, 
                                struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED) { }
static void toplevel_handle_parent(void *data G_GNUC_UNUSED, 
                                  struct zwlr_foreign_toplevel_handle_v1 *toplevel G_GNUC_UNUSED, 
                                  struct zwlr_foreign_toplevel_handle_v1 *parent G_GNUC_UNUSED) { }

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

// Callback para nuevas ventanas - almacenar handle y añadir listener
static void toplevel_manager_handle_toplevel(void *data,
                                           struct zwlr_foreign_toplevel_manager_v1 *manager G_GNUC_UNUSED,
                                           struct zwlr_foreign_toplevel_handle_v1 *toplevel) {
    ShowDesktopWidget *self = SHOWDESKTOP_WIDGET(data);
    
    ToplevelHandle *handle_data = g_malloc0(sizeof(ToplevelHandle));
    handle_data->handle = toplevel;
    handle_data->was_minimized = FALSE;
    
    // CRÍTICO: Añadir listener para rastrear estados
    zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &toplevel_handle_listener, self);
    
    self->toplevel_handles = g_slist_append(self->toplevel_handles, handle_data);
}

static void toplevel_manager_handle_finished(void *data G_GNUC_UNUSED,
                                           struct zwlr_foreign_toplevel_manager_v1 *manager G_GNUC_UNUSED) { }

static const struct zwlr_foreign_toplevel_manager_v1_listener toplevel_manager_listener = {
    .toplevel = toplevel_manager_handle_toplevel,
    .finished = toplevel_manager_handle_finished,
};

// Registry listener
static void registry_global(void *data, struct wl_registry *registry,
                          uint32_t name, const char *interface, uint32_t version) {
    ShowDesktopWidget *self = SHOWDESKTOP_WIDGET(data);
    
    if (strcmp(interface, wl_seat_interface.name) == 0) {
        self->wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, 
                                        MIN(version, 2));
    }
    else if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        self->toplevel_manager = wl_registry_bind(registry, name, 
                                                 &zwlr_foreign_toplevel_manager_v1_interface,
                                                 MIN(version, 3));
        if (self->toplevel_manager) {
            zwlr_foreign_toplevel_manager_v1_add_listener(self->toplevel_manager, 
                                                         &toplevel_manager_listener, 
                                                         self);
        }
    }
}

static void registry_global_remove(void *data G_GNUC_UNUSED, 
                                  struct wl_registry *registry G_GNUC_UNUSED, 
                                  uint32_t name G_GNUC_UNUSED) { }

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

// Conectar a Wayland
static void setup_wayland_connection(ShowDesktopWidget *self) {
    self->wl_display = wl_display_connect(NULL);
    if (!self->wl_display) {
        return;
    }
    
    self->wl_registry = wl_display_get_registry(self->wl_display);
    if (!self->wl_registry) {
        wl_display_disconnect(self->wl_display);
        self->wl_display = NULL;
        return;
    }
    
    wl_registry_add_listener(self->wl_registry, &registry_listener, self);
    wl_display_dispatch(self->wl_display);
    
    // CRÍTICO: Procesar ventanas existentes
    if (self->toplevel_manager) {
        wl_display_roundtrip(self->wl_display);  // Asegurar que se detecten ventanas existentes
    }
    
    // CRÍTICO: Integrar eventos con GTK main loop
    setup_wayland_event_integration(self);
}

// Callback para procesar eventos de Wayland desde GTK main loop
static gboolean wayland_event_callback(GIOChannel *channel G_GNUC_UNUSED, GIOCondition condition, gpointer user_data) {
    ShowDesktopWidget *self = SHOWDESKTOP_WIDGET(user_data);
    
    if (condition & G_IO_IN) {
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
static void setup_wayland_event_integration(ShowDesktopWidget *self) {
    if (!self->wl_display) return;
    
    int wayland_fd = wl_display_get_fd(self->wl_display);
    if (wayland_fd < 0) return;
    
    GIOChannel *wayland_channel = g_io_channel_unix_new(wayland_fd);
    if (!wayland_channel) return;
    
    g_io_channel_set_encoding(wayland_channel, NULL, NULL);
    g_io_channel_set_buffered(wayland_channel, FALSE);
    g_io_channel_set_close_on_unref(wayland_channel, FALSE);
    
    guint watch_id = g_io_add_watch(wayland_channel,
                                   G_IO_IN | G_IO_ERR | G_IO_HUP,
                                   wayland_event_callback,
                                   self);
    
    g_object_set_data(G_OBJECT(self), "wayland-watch-id", GUINT_TO_POINTER(watch_id));
    g_io_channel_unref(wayland_channel);
}

// Minimizar todas las ventanas
static void minimize_all_windows(ShowDesktopWidget *self) {
    if (!self->toplevel_manager) return;
    
    GSList *current = self->toplevel_handles;
    while (current) {
        ToplevelHandle *handle_data = current->data;
        
        // Minimizar TODAS las ventanas visibles
        // (las que ya están minimizadas no se afectan)
        zwlr_foreign_toplevel_handle_v1_set_minimized(handle_data->handle);
        
        current = current->next;
    }
    
    if (self->wl_display) {
        wl_display_flush(self->wl_display);
    }
}

// Restaurar ventanas (activar las que no estaban minimizadas originalmente)
static void restore_windows(ShowDesktopWidget *self) {
    if (!self->toplevel_manager || !self->wl_seat) return;
    
    GSList *current = self->toplevel_handles;
    while (current) {
        ToplevelHandle *handle_data = current->data;
        
        // Activar TODAS las ventanas para restaurar el escritorio
        // El protocolo wlroots maneja automáticamente cuáles deben restaurarse
        zwlr_foreign_toplevel_handle_v1_activate(handle_data->handle, self->wl_seat);
        
        current = current->next;
    }
    
    if (self->wl_display) {
        wl_display_flush(self->wl_display);
    }
}

#endif

// Callback del botón
static void on_show_desktop_clicked(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    ShowDesktopWidget *self = SHOWDESKTOP_WIDGET(user_data);
    
#ifdef HAVE_WLR_PROTOCOLS
    if (self->desktop_shown) {
        // Restaurar ventanas
        restore_windows(self);
        self->desktop_shown = FALSE;
        gtk_widget_remove_css_class(GTK_WIDGET(self->button), "active");
    } else {
        // Minimizar todas las ventanas
        minimize_all_windows(self);
        self->desktop_shown = TRUE;
        gtk_widget_add_css_class(GTK_WIDGET(self->button), "active");
    }
#endif
}

// Aplicar estilos CSS
static void apply_showdesktop_styles(void) {
    static gboolean styles_applied = FALSE;
    if (styles_applied) return;
    
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(provider, "/io/gitlab/sodomon/simple_panel/styles/showdesktop-styles.css");
    
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
    styles_applied = TRUE;
}

// Inicialización de la clase
static void showdesktop_widget_class_init(ShowDesktopWidgetClass *class G_GNUC_UNUSED) {
    // Configuración de la clase
}

// Inicialización de la instancia
static void showdesktop_widget_init(ShowDesktopWidget *self) {
    if (!self) return;
    
    // Configurar como box horizontal
    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(GTK_WIDGET(self), "show-desktop");
    
    // Crear botón
    self->button = gtk_button_new();
    gtk_widget_add_css_class(self->button, "show-desktop-button");
    
    // Icono del botón (mostrar escritorio)
    GtkWidget *icon = gtk_image_new_from_icon_name("user-desktop");
    gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_NORMAL);
    gtk_button_set_child(GTK_BUTTON(self->button), icon);
    
    // Tooltip
    gtk_widget_set_tooltip_text(self->button, "Mostrar escritorio");
    
    // Conectar señal
    g_signal_connect(self->button, "clicked", G_CALLBACK(on_show_desktop_clicked), self);
    
    // Añadir botón al contenedor
    gtk_box_append(GTK_BOX(self), self->button);
    
    // Aplicar estilos
    apply_showdesktop_styles();
    
    // Estado inicial
    self->desktop_shown = FALSE;
    
#ifdef HAVE_WLR_PROTOCOLS
    // Inicializar Wayland
    self->wl_display = NULL;
    self->wl_registry = NULL;
    self->wl_seat = NULL;
    self->toplevel_manager = NULL;
    self->toplevel_handles = NULL;
    
    setup_wayland_connection(self);
#endif
}

// Constructor público
ShowDesktopWidget *showdesktop_widget_new(PanelConfig *config) {
    ShowDesktopWidget *widget = g_object_new(SHOWDESKTOP_TYPE_WIDGET, NULL);
    widget->config = config;
    return widget;
}
