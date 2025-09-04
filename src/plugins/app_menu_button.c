#include "app_menu_button.h"
#include <gio/gdesktopappinfo.h>

typedef struct {
    GtkWidget *popover;
    GMenu *menu_model;
    const gchar *category_name;
} CategoryMenu;

struct _AppMenuButton {
    GtkBox parent_instance;
    GtkWidget *menu_button;
    GtkWidget *main_menu;
    GSList *category_menus;
    GSimpleActionGroup *action_group;
};

G_DEFINE_TYPE(AppMenuButton, app_menu_button, GTK_TYPE_BOX)

typedef struct {
    AppMenuButton *menu_button;
    gchar *app_id;
} LaunchData;

static void launch_data_free(LaunchData *data) {
    if (data) {
        g_free(data->app_id);
        g_free(data);
    }
}


static void hide_all_category_menus(AppMenuButton *self) {
    for (GSList *l = self->category_menus; l != NULL; l = l->next) {
        CategoryMenu *cat_menu = (CategoryMenu *)l->data;
        gtk_popover_popdown(GTK_POPOVER(cat_menu->popover));
    }
}
static void launch_app_callback(GSimpleAction *action G_GNUC_UNUSED, GVariant *parameter G_GNUC_UNUSED, gpointer user_data) {
    LaunchData *data = (LaunchData *)user_data;
    const gchar *app_id = data->app_id;
    AppMenuButton *self = data->menu_button;
    
    GDesktopAppInfo *d_app_info = g_desktop_app_info_new(app_id);
    if (d_app_info) {
        g_app_info_launch(G_APP_INFO(d_app_info), NULL, NULL, NULL);
        g_object_unref(d_app_info);
        
        // Ocultar todos los menús después de lanzar la aplicación
        hide_all_category_menus(self);
        gtk_popover_popdown(GTK_POPOVER(self->main_menu));
    } else {
        g_warning("No se pudo encontrar la aplicación con ID: %s", app_id);
    }
}


static void show_category_menu(AppMenuButton *self, const gchar *category_name, GtkWidget *relative_widget) {
    // Primero ocultar todos los submenús
    hide_all_category_menus(self);
    
    // Buscar y mostrar el menú de la categoría específica
    for (GSList *l = self->category_menus; l != NULL; l = l->next) {
        CategoryMenu *cat_menu = (CategoryMenu *)l->data;
        if (g_strcmp0(cat_menu->category_name, category_name) == 0) {
            // Configurar el popover para que aparezca a la derecha del botón
            gtk_popover_set_position(GTK_POPOVER(cat_menu->popover), GTK_POS_RIGHT);
            
            // Como el parent es el botón, posicionar en todo el ancho/alto del botón
            graphene_rect_t bounds;
            if (gtk_widget_compute_bounds(relative_widget, relative_widget, &bounds)) {
                GdkRectangle rect = {
                    .x = 0,
                    .y = 0,
                    .width = (int)bounds.size.width,
                    .height = (int)bounds.size.height
                };
                gtk_popover_set_pointing_to(GTK_POPOVER(cat_menu->popover), &rect);
            }
            gtk_popover_popup(GTK_POPOVER(cat_menu->popover));
            break;
        }
    }
}

static gboolean on_category_enter(GtkEventControllerMotion *controller, gdouble x G_GNUC_UNUSED, gdouble y G_GNUC_UNUSED, gpointer user_data) {
    GtkWidget *button = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    const gchar *category_name = (const gchar *)g_object_get_data(G_OBJECT(button), "category-name");
    AppMenuButton *self = APP_MENU_BUTTON(user_data);
    
    show_category_menu(self, category_name, button);
    return FALSE;
}

static void on_main_menu_closed(GtkPopover *popover G_GNUC_UNUSED, gpointer user_data) {
    AppMenuButton *self = APP_MENU_BUTTON(user_data);
    hide_all_category_menus(self);
}

static void on_category_menu_closed(GtkPopover *popover G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED) {
    // Callback vacío - los submenús se controlan manualmente
}

static gboolean on_main_menu_click(GtkGestureClick *gesture G_GNUC_UNUSED, gint n_press G_GNUC_UNUSED, gdouble x G_GNUC_UNUSED, gdouble y G_GNUC_UNUSED, gpointer user_data) {
    // Si se hace clic dentro del menú principal pero fuera de los botones, ocultar submenús
    AppMenuButton *self = APP_MENU_BUTTON(user_data);
    hide_all_category_menus(self);
    return FALSE; // Permitir que el evento se propague
}

static void on_menu_button_clicked(GtkButton *button G_GNUC_UNUSED, gpointer user_data) {
    AppMenuButton *self = APP_MENU_BUTTON(user_data);
    gtk_popover_popup(GTK_POPOVER(self->main_menu));
}

static void app_menu_button_init(AppMenuButton *self) {
    // Inicializar listas
    self->category_menus = NULL;
    
    // Crear un botón normal
    self->menu_button = gtk_button_new();
    gtk_button_set_icon_name(GTK_BUTTON(self->menu_button), "start-here-symbolic");
    gtk_widget_set_tooltip_text(self->menu_button, "Menú de Aplicaciones");
    gtk_box_append(GTK_BOX(self), self->menu_button);

    // Crear el menú principal como popover simple con botones
    self->main_menu = gtk_popover_new();
    gtk_widget_set_parent(self->main_menu, self->menu_button);
    gtk_popover_set_position(GTK_POPOVER(self->main_menu), GTK_POS_BOTTOM);
    gtk_popover_set_autohide(GTK_POPOVER(self->main_menu), TRUE); // Auto-ocultar al hacer clic fuera
    gtk_popover_set_has_arrow(GTK_POPOVER(self->main_menu), FALSE);
    gtk_widget_add_css_class(self->main_menu, "menu-popover");
    
    // Box principal para el contenido del menú
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_popover_set_child(GTK_POPOVER(self->main_menu), main_box);
    
    // Añadir controlador de clics al menú principal para ocultar submenús
    GtkGesture *click_gesture = gtk_gesture_click_new();
    g_signal_connect(click_gesture, "pressed", G_CALLBACK(on_main_menu_click), self);
    gtk_widget_add_controller(main_box, GTK_EVENT_CONTROLLER(click_gesture));

    // Aplicar CSS optimizado para estilo clásico
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider,
        /* Base común para popovers */
        "popover.menu-popover, popover.menu-popover > contents {"
        "  border-radius: 0px;"
        "  border: none;"
        "  background: #f8f8f8;"
        "  padding: 0px;"
        "}"
        "popover.menu-popover {"
        "  box-shadow: 2px 2px 8px rgba(0,0,0,0.3);"
        "}"
        /* Base común para botones de menú */
        ".menu-category-button, .menu-app-button {"
        "  border-radius: 0px;"
        "  margin: 0px;"
        "  background: transparent;"
        "  border: none;"
        "  color: #2d2d2d;"
        "}"
        ".menu-category-button:hover, .menu-app-button:hover {"
        "  background: #0078d4;"
        "  color: white;"
        "}"
        /* Estilos específicos */
        ".menu-category-button {"
        "  padding: 6px 12px 6px 8px;"
        "  min-width: 180px;"
        "  font-size: 13px;"
        "}"
        ".menu-app-button {"
        "  padding: 4px 8px 4px 6px;"
        "  min-width: 160px;"
        "  font-size: 12px;"
        "}"
        /* Elementos internos */
        ".menu-category-button label, .menu-category-button image,"
        ".menu-app-button label, .menu-app-button image {"
        "  color: inherit;"
        "}"
    );
    
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    // Crear grupo de acciones para las aplicaciones
    self->action_group = g_simple_action_group_new();
    gtk_widget_insert_action_group(GTK_WIDGET(self), "app", G_ACTION_GROUP(self->action_group));

    // Estructuras de datos para categorías
    GHashTable *categories = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    
    // Lista de categorías importantes
    const gchar *important_categories[] = {
        "AudioVideo", "Development", "Education", "Game",
        "Graphics", "Network", "Office", "System", "Utility", NULL
    };
    
    // Mapeo de categorías técnicas a nombres amigables
    GHashTable *category_names = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(category_names, "AudioVideo", "Audio y Video");
    g_hash_table_insert(category_names, "Development", "Desarrollo"); 
    g_hash_table_insert(category_names, "Education", "Educación");
    g_hash_table_insert(category_names, "Game", "Juegos");
    g_hash_table_insert(category_names, "Graphics", "Gráficos");
    g_hash_table_insert(category_names, "Network", "Internet");
    g_hash_table_insert(category_names, "Office", "Oficina");
    g_hash_table_insert(category_names, "System", "Sistema");
    g_hash_table_insert(category_names, "Utility", "Utilidades");
    
    // Mapeo de iconos para categorías
    GHashTable *category_icons = g_hash_table_new(g_str_hash, g_str_equal);
    g_hash_table_insert(category_icons, "AudioVideo", "multimedia-volume-control");
    g_hash_table_insert(category_icons, "Development", "applications-development");
    g_hash_table_insert(category_icons, "Education", "applications-science");
    g_hash_table_insert(category_icons, "Game", "applications-games");
    g_hash_table_insert(category_icons, "Graphics", "applications-graphics");
    g_hash_table_insert(category_icons, "Network", "applications-internet");
    g_hash_table_insert(category_icons, "Office", "applications-office");
    g_hash_table_insert(category_icons, "System", "applications-system");
    g_hash_table_insert(category_icons, "Utility", "applications-utilities");

    // Recopilar aplicaciones por categoría
    GList *app_infos = g_app_info_get_all();
    for (GList *l = app_infos; l != NULL; l = l->next) {
        GAppInfo *app_info = l->data;
        if (!G_IS_DESKTOP_APP_INFO(app_info) || !g_app_info_should_show(app_info)) {
            continue;
        }
        GDesktopAppInfo *d_app_info = G_DESKTOP_APP_INFO(app_info);

        const gchar *categories_str = g_desktop_app_info_get_categories(d_app_info);
        if (!categories_str) continue;

        gchar **categories_split = g_strsplit(categories_str, ";", -1);
        if (!categories_split || !categories_split[0] || categories_split[0][0] == '\0') {
            g_strfreev(categories_split);
            continue;
        }

        // Buscar la primera categoría importante
        const gchar *main_category = NULL;
        for (int i = 0; categories_split[i]; i++) {
            for (int j = 0; important_categories[j]; j++) {
                if (g_strcmp0(categories_split[i], important_categories[j]) == 0) {
                    main_category = important_categories[j];
                    break;
                }
            }
            if (main_category) break;
        }

        if (!main_category) {
            g_strfreev(categories_split);
            continue;
        }

        // Obtener o crear lista de apps para la categoría
        GList *app_list = g_hash_table_lookup(categories, main_category);
        app_list = g_list_append(app_list, g_object_ref(app_info));
        g_hash_table_replace(categories, g_strdup(main_category), app_list);

        g_strfreev(categories_split);
    }

    // Crear botones de categoría y submenús
    GHashTableIter iter;
    gpointer key, value;
    g_hash_table_iter_init(&iter, categories);
    
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const gchar *category_key = (const gchar *)key;
        GList *app_list = (GList *)value;
        
        const gchar *friendly_name = g_hash_table_lookup(category_names, category_key);
        if (!friendly_name) friendly_name = category_key;
        
        // Crear botón de categoría con icono en el menú principal
        GtkWidget *category_button = gtk_button_new();
        gtk_widget_add_css_class(category_button, "menu-category-button");
        
        // Crear box horizontal para icono + texto
        GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_halign(button_box, GTK_ALIGN_START);
        
        // Agregar icono
        const gchar *icon_name = g_hash_table_lookup(category_icons, category_key);
        if (!icon_name) icon_name = "folder";
        GtkWidget *icon = gtk_image_new_from_icon_name(icon_name);
        gtk_image_set_icon_size(GTK_IMAGE(icon), GTK_ICON_SIZE_NORMAL);
        gtk_box_append(GTK_BOX(button_box), icon);
        
        // Agregar etiqueta
        GtkWidget *label = gtk_label_new(friendly_name);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_append(GTK_BOX(button_box), label);
        
        gtk_button_set_child(GTK_BUTTON(category_button), button_box);
        gtk_box_append(GTK_BOX(main_box), category_button);
        
        // Almacenar nombre de categoría en el botón
        g_object_set_data_full(G_OBJECT(category_button), "category-name", 
                              g_strdup(category_key), g_free);
        
        // Añadir controlador de movimiento del ratón
        GtkEventController *motion_controller = gtk_event_controller_motion_new();
        g_signal_connect(motion_controller, "enter", G_CALLBACK(on_category_enter), self);
        gtk_widget_add_controller(category_button, motion_controller);
        
        // Crear popover para esta categoría
        GtkWidget *category_popover = gtk_popover_new();
        // Usar el botón de categoría como parent para mejor posicionamiento
        gtk_widget_set_parent(category_popover, category_button);
        gtk_popover_set_autohide(GTK_POPOVER(category_popover), FALSE); // Desactivar autohide para evitar conflictos
        gtk_popover_set_has_arrow(GTK_POPOVER(category_popover), FALSE);
        gtk_widget_add_css_class(category_popover, "menu-popover");
        
        GtkWidget *category_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_popover_set_child(GTK_POPOVER(category_popover), category_box);
        
        // Conectar señal de cierre del submenu
        g_signal_connect(category_popover, "closed", G_CALLBACK(on_category_menu_closed), self);
        
        // Añadir aplicaciones al submenu
        for (GList *app_iter = app_list; app_iter != NULL; app_iter = app_iter->next) {
            GAppInfo *app_info = G_APP_INFO(app_iter->data);
            const gchar *app_id = g_app_info_get_id(app_info);
            if (!app_id) continue;
            
            // Crear botón de aplicación con icono
            GtkWidget *app_button = gtk_button_new();
            gtk_widget_add_css_class(app_button, "menu-app-button");
            
            // Crear box horizontal para icono + texto
            GtkWidget *app_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_halign(app_button_box, GTK_ALIGN_START);
            
            // Agregar icono de la aplicación
            GIcon *app_icon = g_app_info_get_icon(app_info);
            GtkWidget *icon_widget;
            if (app_icon) {
                icon_widget = gtk_image_new_from_gicon(app_icon);
            } else {
                icon_widget = gtk_image_new_from_icon_name("application-x-executable");
            }
            gtk_image_set_icon_size(GTK_IMAGE(icon_widget), GTK_ICON_SIZE_NORMAL);
            gtk_box_append(GTK_BOX(app_button_box), icon_widget);
            
            // Agregar etiqueta de la aplicación
            GtkWidget *app_label = gtk_label_new(g_app_info_get_display_name(app_info));
            gtk_label_set_xalign(GTK_LABEL(app_label), 0.0);
            gtk_box_append(GTK_BOX(app_button_box), app_label);
            
            gtk_button_set_child(GTK_BUTTON(app_button), app_button_box);
            gtk_box_append(GTK_BOX(category_box), app_button);
            
            // Crear acción para lanzar aplicación
            gchar *action_name = g_strconcat("launch.", app_id, NULL);
            if (!g_action_map_lookup_action(G_ACTION_MAP(self->action_group), action_name)) {
                GSimpleAction *action = g_simple_action_new(action_name, NULL);
                
                // Crear estructura LaunchData para pasar al callback
                LaunchData *launch_data = g_malloc(sizeof(LaunchData));
                launch_data->menu_button = self;
                launch_data->app_id = g_strdup(app_id);
                
                g_signal_connect(action, "activate", G_CALLBACK(launch_app_callback), launch_data);
                g_action_map_add_action(G_ACTION_MAP(self->action_group), G_ACTION(action));
                
                // Almacenar datos para limpieza posterior
                g_object_set_data_full(G_OBJECT(action), "launch-data", launch_data, 
                                      (GDestroyNotify)launch_data_free);
            }
            
            gchar *full_action_name = g_strconcat("app.", action_name, NULL);
            gtk_actionable_set_action_name(GTK_ACTIONABLE(app_button), full_action_name);
            
            g_free(action_name);
            g_free(full_action_name);
        }
        
        // Almacenar información del submenu
        CategoryMenu *cat_menu = g_malloc(sizeof(CategoryMenu));
        cat_menu->popover = category_popover;
        cat_menu->category_name = g_strdup(category_key);
        cat_menu->menu_model = NULL; // No usado en este enfoque
        
        self->category_menus = g_slist_append(self->category_menus, cat_menu);
    }
    
    // Conectar señales
    g_signal_connect(self->menu_button, "clicked", G_CALLBACK(on_menu_button_clicked), self);
    g_signal_connect(self->main_menu, "closed", G_CALLBACK(on_main_menu_closed), self);
    
    // Limpieza
    g_list_free_full(app_infos, g_object_unref);
    g_hash_table_destroy(categories);
    g_hash_table_destroy(category_names);
    g_hash_table_destroy(category_icons);
    g_object_unref(css_provider);
}

static void app_menu_button_dispose(GObject *object) {
    AppMenuButton *self = APP_MENU_BUTTON(object);
    
    // Limpiar menús de categoría
    for (GSList *l = self->category_menus; l != NULL; l = l->next) {
        CategoryMenu *cat_menu = (CategoryMenu *)l->data;
        if (cat_menu->popover) {
            gtk_widget_unparent(cat_menu->popover);
        }
        g_free((gpointer)cat_menu->category_name);
        g_free(cat_menu);
    }
    g_slist_free(self->category_menus);
    self->category_menus = NULL;
    
    // Limpiar menú principal
    if (self->main_menu) {
        gtk_widget_unparent(self->main_menu);
        self->main_menu = NULL;
    }
    
    G_OBJECT_CLASS(app_menu_button_parent_class)->dispose(object);
}

static void app_menu_button_class_init(AppMenuButtonClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = app_menu_button_dispose;
}

GtkWidget *app_menu_button_new(const gchar *icon_name) {
    AppMenuButton *self = APP_MENU_BUTTON(g_object_new(APP_TYPE_MENU_BUTTON, NULL));
    
    // Cambiar el icono si se especifica uno diferente del por defecto
    if (icon_name && g_strcmp0(icon_name, "start-here-symbolic") != 0) {
        gtk_button_set_icon_name(GTK_BUTTON(self->menu_button), icon_name);
    }
    
    return GTK_WIDGET(self);
}
