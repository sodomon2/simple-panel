#include "systray_widget.h"
#include <gdk/gdk.h>

// StatusNotifierItem DBus interface definitions
#define WATCHER_SERVICE "org.kde.StatusNotifierWatcher"
#define WATCHER_PATH "/StatusNotifierWatcher"
#define WATCHER_INTERFACE "org.kde.StatusNotifierWatcher"

#define ITEM_INTERFACE "org.kde.StatusNotifierItem"

// Estructura para cada elemento del system tray
typedef struct {
    gchar *service_name;
    gchar *object_path;
    gchar *id;
    gchar *title;
    gchar *icon_name;
    gchar *icon_theme_path;
    gchar *tooltip_title;
    gchar *tooltip_text;
    GtkWidget *button;
    GtkWidget *icon_widget;
    GDBusProxy *proxy;
    guint name_watcher_id;
    SystrayWidget *systray;
} TrayItem;

struct _SystrayWidget {
    GtkBox parent_instance;
    
    PanelConfig *config;
    GDBusConnection *dbus_connection;
    
    // Como somos nuestro propio watcher, no necesitamos proxy externo
    guint watcher_registration_id;
    
    // Lista de elementos activos del tray
    GSList *tray_items;
    GSList *registered_items; // Lista de servicios registrados
    
    // Watchers para detectar nuevos servicios DBus
    guint name_watcher_id;
};

G_DEFINE_TYPE(SystrayWidget, systray_widget, GTK_TYPE_BOX)

// Forward declaration
static void remove_tray_item(SystrayWidget *systray, const gchar *service_name);

// Liberar memoria de un tray item
static void tray_item_free(TrayItem *item) {
    if (!item) return;
    
    g_free(item->service_name);
    g_free(item->object_path);
    g_free(item->id);
    g_free(item->title);
    g_free(item->icon_name);
    g_free(item->icon_theme_path);
    g_free(item->tooltip_title);
    g_free(item->tooltip_text);
    
    if (item->proxy) {
        g_object_unref(item->proxy);
    }
    
    if (item->button) {
        gtk_widget_unparent(item->button);
    }
    
    // Limpiar el watcher de nombre DBus
    if (item->name_watcher_id > 0) {
        g_bus_unwatch_name(item->name_watcher_id);
    }
    
    g_free(item);
}

// Callback para cuando se hace click en un tray icon
static void on_tray_item_clicked(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    TrayItem *item = (TrayItem *)user_data;
    
    if (!item->proxy) return;
    
    // Llamar método Activate del StatusNotifierItem
    g_dbus_proxy_call(item->proxy,
                     "Activate",
                     g_variant_new("(ii)", 0, 0), // x, y coordinates
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
}

// Callback para click derecho en tray icon
static gboolean on_tray_item_right_click(GtkGestureClick *gesture G_GNUC_UNUSED, 
                                         gint n_press G_GNUC_UNUSED, 
                                         gdouble x G_GNUC_UNUSED, 
                                         gdouble y G_GNUC_UNUSED, 
                                         gpointer user_data) {
    TrayItem *item = (TrayItem *)user_data;
    
    if (!item->proxy) return FALSE;
    
    // Llamar método ContextMenu del StatusNotifierItem
    g_dbus_proxy_call(item->proxy,
                     "ContextMenu",
                     g_variant_new("(ii)", 0, 0), // x, y coordinates
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     NULL,
                     NULL);
    
    return TRUE;
}

// Actualizar icono de un tray item
static void update_tray_item_icon(TrayItem *item) {
    if (!item->icon_widget || !item->icon_name) return;
    
    // Intentar cargar el icono desde el tema
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_display(gdk_display_get_default());
    
    if (gtk_icon_theme_has_icon(icon_theme, item->icon_name)) {
        GtkIconPaintable *icon = gtk_icon_theme_lookup_icon(icon_theme,
                                                           item->icon_name,
                                                           NULL, // fallbacks
                                                           22, // size
                                                           1,  // scale
                                                           GTK_TEXT_DIR_NONE,
                                                           0); // flags
        if (icon) {
            gtk_image_set_from_paintable(GTK_IMAGE(item->icon_widget), GDK_PAINTABLE(icon));
            g_object_unref(icon);
        }
    } else {
        // Fallback: usar icono genérico
        gtk_image_set_from_icon_name(GTK_IMAGE(item->icon_widget), "application-x-executable");
    }
}

// Actualizar propiedades de un tray item desde DBus
static void update_tray_item_properties(TrayItem *item) {
    if (!item->proxy) return;
    
    GVariant *result;
    GError *error = NULL;
    
    // Obtener todas las propiedades del StatusNotifierItem
    result = g_dbus_proxy_call_sync(item->proxy,
                                   "org.freedesktop.DBus.Properties.GetAll",
                                   g_variant_new("(s)", ITEM_INTERFACE),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   5000, // 5 second timeout
                                   NULL,
                                   &error);
    
    if (error) {
        g_warning("Error al obtener propiedades del tray item %s: %s", 
                 item->service_name, error->message);
        g_error_free(error);
        return;
    }
    
    if (result) {
        GVariantDict dict;
        g_variant_dict_init(&dict, g_variant_get_child_value(result, 0));
        
        // Actualizar propiedades
        GVariant *value;
        
        if ((value = g_variant_dict_lookup_value(&dict, "Id", G_VARIANT_TYPE_STRING))) {
            g_free(item->id);
            item->id = g_variant_dup_string(value, NULL);
            g_variant_unref(value);
        }
        
        if ((value = g_variant_dict_lookup_value(&dict, "Title", G_VARIANT_TYPE_STRING))) {
            g_free(item->title);
            item->title = g_variant_dup_string(value, NULL);
            g_variant_unref(value);
        }
        
        if ((value = g_variant_dict_lookup_value(&dict, "IconName", G_VARIANT_TYPE_STRING))) {
            g_free(item->icon_name);
            item->icon_name = g_variant_dup_string(value, NULL);
            update_tray_item_icon(item);
            g_variant_unref(value);
        }
        
        if ((value = g_variant_dict_lookup_value(&dict, "ToolTip", G_VARIANT_TYPE("(sa(iiay)ss)")))) {
            // Tooltip es una estructura compleja, solo tomamos el título por simplicidad
            GVariant *tooltip_title = g_variant_get_child_value(value, 2);
            g_free(item->tooltip_title);
            item->tooltip_title = g_variant_dup_string(tooltip_title, NULL);
            
            if (item->button && item->tooltip_title && strlen(item->tooltip_title) > 0) {
                gtk_widget_set_tooltip_text(item->button, item->tooltip_title);
            }
            
            g_variant_unref(tooltip_title);
            g_variant_unref(value);
        }
        
        g_variant_dict_clear(&dict);
        g_variant_unref(result);
    }
}

// Crear widget para un tray item
static void create_tray_item_widget(TrayItem *item) {
    // Crear botón para el tray item
    item->button = gtk_button_new();
    gtk_widget_add_css_class(item->button, "systray-item");
    
    // Crear imagen para el icono
    item->icon_widget = gtk_image_new();
    gtk_button_set_child(GTK_BUTTON(item->button), item->icon_widget);
    
    // Conectar eventos
    g_signal_connect(item->button, "clicked", G_CALLBACK(on_tray_item_clicked), item);
    
    // Agregar gestión de click derecho
    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click), GDK_BUTTON_SECONDARY);
    g_signal_connect(right_click, "pressed", G_CALLBACK(on_tray_item_right_click), item);
    gtk_widget_add_controller(item->button, GTK_EVENT_CONTROLLER(right_click));
    
    // Actualizar propiedades y mostrar
    update_tray_item_properties(item);
    gtk_box_append(GTK_BOX(item->systray), item->button);
}

// Callback cuando un servicio DBus desaparece
static void on_name_vanished(GDBusConnection *connection G_GNUC_UNUSED, 
                            const gchar *name,
                            gpointer user_data) {
    SystrayWidget *systray = SYSTRAY_WIDGET(user_data);
    g_print("DBus name vanished: %s\n", name);
    remove_tray_item(systray, name);
}

// Crear nuevo tray item
static TrayItem *create_tray_item(SystrayWidget *systray, const gchar *service_name) {
    TrayItem *item = g_malloc0(sizeof(TrayItem));
    item->systray = systray;
    
    // Parsear service_name (formato: "service" o "service/path")
    if (g_strrstr(service_name, "/")) {
        gchar **parts = g_strsplit(service_name, "/", 2);
        item->service_name = g_strdup(parts[0]);
        item->object_path = g_strdup_printf("/%s", parts[1]);
        g_strfreev(parts);
    } else {
        item->service_name = g_strdup(service_name);
        item->object_path = g_strdup("/StatusNotifierItem");
    }
    
    // Crear proxy DBus para el StatusNotifierItem
    GError *error = NULL;
    item->proxy = g_dbus_proxy_new_sync(systray->dbus_connection,
                                       G_DBUS_PROXY_FLAGS_NONE, NULL,
                                       item->service_name, item->object_path,
                                       ITEM_INTERFACE, NULL, &error);
    
    if (error) {
        g_warning("Error conectando a %s%s: %s", item->service_name, item->object_path, error->message);
        g_error_free(error);
        tray_item_free(item);
        return NULL;
    }
    
    // Monitorear este servicio específico para detectar cuando se desconecta
    guint watcher_id = g_bus_watch_name_on_connection(systray->dbus_connection,
                                                     item->service_name,
                                                     G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                     NULL, // name_appeared
                                                     on_name_vanished,
                                                     systray,
                                                     NULL);
    
    // Almacenar el watcher ID para poder limpiarlo después
    item->name_watcher_id = watcher_id;
    
    create_tray_item_widget(item);
    systray->tray_items = g_slist_append(systray->tray_items, item);
    g_print("✓ Tray item: %s%s (monitoring: %u)\n", item->service_name, item->object_path, watcher_id);
    return item;
}

// Remover tray item
static void remove_tray_item(SystrayWidget *systray, const gchar *service_name) {
    for (GSList *l = systray->tray_items; l != NULL; l = l->next) {
        TrayItem *item = (TrayItem *)l->data;
        if (g_strcmp0(item->service_name, service_name) == 0) {
            systray->tray_items = g_slist_remove(systray->tray_items, item);
            tray_item_free(item);
            g_print("- Tray item removido: %s\n", service_name);
            break;
        }
    }
    
    // También remover de la lista de servicios registrados
    for (GSList *l = systray->registered_items; l != NULL; l = l->next) {
        if (g_strcmp0((gchar *)l->data, service_name) == 0) {
            g_free(l->data);
            systray->registered_items = g_slist_remove(systray->registered_items, l->data);
            break;
        }
    }
    
    // Emitir señal de desregistro
    g_dbus_connection_emit_signal(systray->dbus_connection, NULL, WATCHER_PATH, WATCHER_INTERFACE,
                                 "StatusNotifierItemUnregistered", g_variant_new("(s)", service_name), NULL);
}

// Cargar XML de introspección desde recursos
static gchar *load_watcher_introspection_xml(void) {
    GBytes *xml_bytes = g_resources_lookup_data(
        "/io/gitlab/sodomon/simple_panel/status_service/service/StatusNotifierWatcher.xml",
        G_RESOURCE_LOOKUP_FLAGS_NONE,
        NULL
    );
    
    if (!xml_bytes) {
        g_warning("No se pudo cargar StatusNotifierWatcher.xml desde recursos");
        return NULL;
    }
    
    gsize size;
    const gchar *data = g_bytes_get_data(xml_bytes, &size);
    gchar *xml_string = g_strndup(data, size);
    g_bytes_unref(xml_bytes);
    
    return xml_string;
}

// Handler consolidado para métodos DBus
static void handle_watcher_method(const gchar *method_name, const gchar *sender,
                                 GVariant *parameters, GDBusMethodInvocation *invocation,
                                 SystrayWidget *systray) {
    if (g_strcmp0(method_name, "RegisterStatusNotifierItem") == 0) {
        const gchar *service_name;
        g_variant_get(parameters, "(&s)", &service_name);
        
        if (!service_name || strlen(service_name) == 0) service_name = sender;
        
        systray->registered_items = g_slist_append(systray->registered_items, g_strdup(service_name));
        create_tray_item(systray, service_name);
        
        g_dbus_connection_emit_signal(systray->dbus_connection, NULL, WATCHER_PATH, WATCHER_INTERFACE,
                                     "StatusNotifierItemRegistered", g_variant_new("(s)", service_name), NULL);
        g_print("+ Item: %s\n", service_name);
    }
    
    g_dbus_method_invocation_return_value(invocation, NULL);
}

// Handler de propiedades DBus simplificado
static GVariant *handle_get_property(GDBusConnection *connection G_GNUC_UNUSED,
                                    const gchar *sender G_GNUC_UNUSED,
                                    const gchar *object_path G_GNUC_UNUSED, 
                                    const gchar *interface_name G_GNUC_UNUSED,
                                    const gchar *property_name,
                                    GError **error G_GNUC_UNUSED,
                                    gpointer user_data) {
    SystrayWidget *systray = SYSTRAY_WIDGET(user_data);
    
    if (g_strcmp0(property_name, "RegisteredStatusNotifierItems") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        for (GSList *l = systray->registered_items; l; l = l->next) {
            g_variant_builder_add(&builder, "s", (gchar *)l->data);
        }
        return g_variant_builder_end(&builder);
    }
    
    return g_strcmp0(property_name, "IsStatusNotifierHostRegistered") == 0 ? 
           g_variant_new_boolean(TRUE) : NULL;
}

// Dispatcher principal de métodos DBus
static void watcher_method_call(GDBusConnection *connection G_GNUC_UNUSED,
                               const gchar *sender, const gchar *object_path G_GNUC_UNUSED,
                               const gchar *interface_name G_GNUC_UNUSED, const gchar *method_name,
                               GVariant *parameters, GDBusMethodInvocation *invocation,
                               gpointer user_data) {
    handle_watcher_method(method_name, sender, parameters, invocation, SYSTRAY_WIDGET(user_data));
}

static const GDBusInterfaceVTable watcher_interface_vtable = {
    .method_call = watcher_method_call,
    .get_property = handle_get_property,
    .set_property = NULL
};

// Forward declarations
static void discover_existing_tray_items(SystrayWidget *systray);

// Callbacks simplificados para DBus
static void on_watcher_registered(GDBusConnection *connection G_GNUC_UNUSED, const gchar *name G_GNUC_UNUSED, gpointer user_data) {
    SystrayWidget *systray = SYSTRAY_WIDGET(user_data);
    
    // Emitir señal de que el host está registrado
    g_dbus_connection_emit_signal(systray->dbus_connection, NULL, WATCHER_PATH, WATCHER_INTERFACE, "StatusNotifierHostRegistered", NULL, NULL);
    
    // CRÍTICO: Buscar aplicaciones existentes que ya estén ejecutándose
    discover_existing_tray_items(systray);
}

static void on_watcher_name_lost(GDBusConnection *connection G_GNUC_UNUSED, const gchar *name, gpointer user_data G_GNUC_UNUSED) {
    g_warning("Perdido nombre DBus: %s", name);
}

// Registrar StatusNotifierWatcher en DBus usando recursos XML
static void register_watcher_service(SystrayWidget *systray) {
    GError *error = NULL;
    
    // Cargar XML desde recursos
    gchar *xml_data = load_watcher_introspection_xml();
    if (!xml_data) {
        g_warning("No se pudo cargar XML de introspección");
        return;
    }
    
    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml(xml_data, &error);
    g_free(xml_data);
    
    if (error) {
        g_warning("Error parseando XML: %s", error->message);
        g_error_free(error);
        return;
    }
    
    systray->watcher_registration_id = g_dbus_connection_register_object(systray->dbus_connection, WATCHER_PATH,
        introspection_data->interfaces[0], &watcher_interface_vtable, systray, NULL, &error);
    
    if (!error) {
        g_bus_own_name_on_connection(systray->dbus_connection, WATCHER_SERVICE, G_BUS_NAME_OWNER_FLAGS_NONE,
                                    on_watcher_registered, on_watcher_name_lost, systray, NULL);
    } else {
        g_warning("Error registrando objeto DBus: %s", error->message);
        g_error_free(error);
    }
    
    g_dbus_node_info_unref(introspection_data);
}

// Inicializar conexión DBus
static void init_dbus_connection(SystrayWidget *systray) {
    GError *error = NULL;
    
    systray->dbus_connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (error) {
        g_warning("Error al conectar con DBus: %s", error->message);
        g_error_free(error);
        return;
    }
    
    // Registrar nuestro propio StatusNotifierWatcher service
    register_watcher_service(systray);
}

static void systray_widget_init(SystrayWidget *self) {
    self->tray_items = NULL;
    
    // Aplicar CSS para el systray
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css_provider, "/io/gitlab/sodomon/simple_panel/styles/systray-styles.css");
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    g_object_unref(css_provider);
    
    init_dbus_connection(self);
}

// Detectar automáticamente aplicaciones existentes con StatusNotifierItem
static void discover_existing_tray_items(SystrayWidget *systray) {
    if (!systray->dbus_connection) return;
    
    GError *error = NULL;
    GVariant *result = g_dbus_connection_call_sync(
        systray->dbus_connection,
        "org.freedesktop.DBus",  // DBus daemon
        "/org/freedesktop/DBus", 
        "org.freedesktop.DBus",
        "ListNames",             // Obtener lista de todos los servicios
        NULL,
        G_VARIANT_TYPE("(as)"),
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return;
    }
    
    GVariantIter *iter;
    g_variant_get(result, "(as)", &iter);
    
    const gchar *service_name;
    while (g_variant_iter_loop(iter, "s", &service_name)) {
        // Buscar servicios que implementen StatusNotifierItem
        if (g_str_has_prefix(service_name, ":") ||  // Skip unique names
            g_strcmp0(service_name, "org.freedesktop.DBus") == 0 ||
            g_strcmp0(service_name, WATCHER_SERVICE) == 0) {
            continue;
        }
        
        // Verificar si el servicio implementa StatusNotifierItem
        GError *introspect_error = NULL;
        GVariant *introspect_result = g_dbus_connection_call_sync(
            systray->dbus_connection,
            service_name,
            "/StatusNotifierItem",  // Path estándar
            "org.freedesktop.DBus.Introspectable",
            "Introspect",
            NULL,
            G_VARIANT_TYPE("(s)"),
            G_DBUS_CALL_FLAGS_NONE,
            1000,  // 1 second timeout
            NULL,
            &introspect_error
        );
        
        if (introspect_result) {
            const gchar *xml_data;
            g_variant_get(introspect_result, "(&s)", &xml_data);
            
            // Verificar si contiene la interfaz StatusNotifierItem
            if (g_strstr_len(xml_data, -1, "org.kde.StatusNotifierItem")) {
                // ¡Encontrada aplicación existente! Registrarla automáticamente
                systray->registered_items = g_slist_append(systray->registered_items, g_strdup(service_name));
                create_tray_item(systray, service_name);
                
                // Emitir señal de registro
                g_dbus_connection_emit_signal(systray->dbus_connection, NULL, WATCHER_PATH, WATCHER_INTERFACE,
                                             "StatusNotifierItemRegistered", g_variant_new("(s)", service_name), NULL);
            }
            
            g_variant_unref(introspect_result);
        }
        
        if (introspect_error) {
            g_error_free(introspect_error);
        }
    }
    
    g_variant_iter_free(iter);
    g_variant_unref(result);
}

static void systray_widget_dispose(GObject *object) {
    SystrayWidget *self = SYSTRAY_WIDGET(object);
    
    // Limpiar tray items
    for (GSList *l = self->tray_items; l != NULL; l = l->next) {
        tray_item_free((TrayItem *)l->data);
    }
    g_slist_free(self->tray_items);
    self->tray_items = NULL;
    
    // Limpiar lista de servicios registrados
    for (GSList *l = self->registered_items; l != NULL; l = l->next) {
        g_free(l->data);
    }
    g_slist_free(self->registered_items);
    self->registered_items = NULL;
    
    // Desregistrar nuestro watcher service
    if (self->watcher_registration_id > 0) {
        g_dbus_connection_unregister_object(self->dbus_connection, self->watcher_registration_id);
        self->watcher_registration_id = 0;
    }
    
    // Limpiar conexión DBus
    if (self->dbus_connection) {
        g_object_unref(self->dbus_connection);
        self->dbus_connection = NULL;
    }
    
    G_OBJECT_CLASS(systray_widget_parent_class)->dispose(object);
}

static void systray_widget_class_init(SystrayWidgetClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = systray_widget_dispose;
}

GtkWidget *systray_widget_new(PanelConfig *config) {
    SystrayWidget *self = g_object_new(SYSTRAY_TYPE_WIDGET, NULL);
    self->config = config;
    return GTK_WIDGET(self);
}
