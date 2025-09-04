#include "app_menu_button.h"
#include "../config.h"
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
    PanelConfig *config;
};

G_DEFINE_TYPE(AppMenuButton, app_menu_button, GTK_TYPE_BOX)

typedef struct {
    AppMenuButton *menu_button;
    gchar *app_id;
} LaunchData;

static void hide_all_category_menus(AppMenuButton *self);

static void launch_data_free(LaunchData *data) {
    if (data) {
        g_free(data->app_id);
        g_free(data);
    }
}

static void execute_system_command(const gchar *command) {
    if (!command || strlen(command) == 0) return;
    
    GError *error = NULL;
    
    // USAR SHELL para manejar variables de entorno, paths ~ y pipes
    gchar *shell_command = g_strdup_printf("sh -c '%s'", command);
    gboolean success = g_spawn_command_line_async(shell_command, &error);
    
    if (!success) {
        g_print("ERROR ejecutando '%s': %s\n", command, error ? error->message : "Unknown");
        if (error) {
            g_error_free(error);
        }
    } else {
        g_print("Ejecutando: %s\n", command);
    }
    
    g_free(shell_command);
}

// CALLBACK SIMPLE Y DIRECTO para comandos del sistema
static void on_system_command_clicked(GtkButton *button, gpointer user_data) {
    AppMenuButton *self = APP_MENU_BUTTON(user_data);
    
    // Obtener comando directamente del botón
    const gchar *command = g_object_get_data(G_OBJECT(button), "command");
    
    if (command && strlen(command) > 0) {
        execute_system_command(command);
        
        // Ocultar menú después de ejecutar comando
        hide_all_category_menus(self);
        gtk_popover_popdown(GTK_POPOVER(self->main_menu));
    }
}

static GtkWidget* create_computer_submenu(AppMenuButton *self) {
    if (!self->config) return NULL;
    
    GtkWidget *computer_menu = gtk_popover_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_popover_set_child(GTK_POPOVER(computer_menu), box);
    
    struct {
        const gchar *name;
        const gchar *label;
        const gchar *icon;
        const gchar *command;
    } system_commands[] = {
        {"lock", "Lock", "system-lock-screen", self->config->lock_cmd},
        {"suspend", "Suspend", "system-suspend", self->config->suspend_cmd},
        {NULL, NULL, NULL, NULL},
        {"logout", "Logout", "system-log-out", self->config->logout_cmd},
        {"reboot", "Reboot", "system-reboot", self->config->reboot_cmd},
        {"poweroff", "Shutdown", "system-shutdown", self->config->poweroff_cmd},
    };
    
    int total_commands = sizeof(system_commands) / sizeof(system_commands[0]);
    for (int i = 0; i < total_commands; i++) {
        if (system_commands[i].name == NULL) {
            GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
            gtk_box_append(GTK_BOX(box), separator);
            continue;
        }
        
        GtkWidget *button = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(button), FALSE);
        gtk_widget_add_css_class(button, "menu-app-button");
        
        // Crear contenido del botón con icono y texto
        GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        
        GtkWidget *icon = gtk_image_new_from_icon_name(system_commands[i].icon);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 16);
        gtk_box_append(GTK_BOX(button_box), icon);
        
        GtkWidget *label = gtk_label_new(system_commands[i].label);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_append(GTK_BOX(button_box), label);
        
        gtk_button_set_child(GTK_BUTTON(button), button_box);
        
        g_object_set_data_full(G_OBJECT(button), "command", 
                              g_strdup(system_commands[i].command), g_free);
        g_signal_connect(button, "clicked", G_CALLBACK(on_system_command_clicked), self);
        gtk_box_append(GTK_BOX(box), button);
    }
    
    return computer_menu;
}

// Categorías estándar - enfoque simple como fbpanel
static struct {
    const gchar *name;
    const gchar *display_name;
    const gchar *icon;
} app_categories[] = {
    {"AudioVideo", "Audio y Video", "applications-multimedia"},
    {"Development", "Desarrollo", "applications-development"},
    {"Education", "Educación", "applications-science"},
    {"Game", "Juegos", "applications-games"},
    {"Graphics", "Gráficos", "applications-graphics"},
    {"Network", "Internet", "applications-internet"},
    {"Office", "Oficina", "applications-office"},
    {"Settings", "Configuración", "preferences-system"},
    {"System", "Sistema", "applications-system"},
    {"Utility", "Utilidades", "applications-utilities"},
    {NULL, NULL, NULL}
};


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
    g_object_unref(css_provider);
}

// 🚀 Construir menú DESPUÉS de establecer config
static void build_main_menu(AppMenuButton *self) {
    if (!self->main_menu) return;
    
    // Obtener el main_box del popover
    GtkWidget *main_box = gtk_popover_get_child(GTK_POPOVER(self->main_menu));

    // 🚀 CREAR CATEGORÍAS SIMPLES - Enfoque fbpanel
    GHashTable *categories = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
    
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

        // Buscar la primera categoría que coincida
        const gchar *main_category = NULL;
        for (int i = 0; categories_split[i]; i++) {
            for (int j = 0; app_categories[j].name; j++) {
                if (g_strcmp0(categories_split[i], app_categories[j].name) == 0) {
                    main_category = app_categories[j].name;
                    break;
                }
            }
            if (main_category) break;
        }

        if (main_category) {
            // Obtener o crear lista de apps para la categoría
            GList *app_list = g_hash_table_lookup(categories, main_category);
            app_list = g_list_append(app_list, g_object_ref(app_info));
            g_hash_table_replace(categories, g_strdup(main_category), app_list);
        }

        g_strfreev(categories_split);
    }
    
    // Crear botones de categoría
    for (int i = 0; app_categories[i].name; i++) {
        const gchar *category_key = app_categories[i].name;
        GList *app_list = g_hash_table_lookup(categories, category_key);
        
        if (!app_list) continue; // Solo mostrar categorías con aplicaciones
        
        // Crear botón de categoría
        GtkWidget *category_button = gtk_button_new();
        gtk_button_set_has_frame(GTK_BUTTON(category_button), FALSE);
        gtk_widget_add_css_class(category_button, "menu-category-button");
        
        GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        
        // Icono
        GtkWidget *icon = gtk_image_new_from_icon_name(app_categories[i].icon);
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 16);
        gtk_box_append(GTK_BOX(button_box), icon);
        
        // Etiqueta
        GtkWidget *label = gtk_label_new(app_categories[i].display_name);
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_append(GTK_BOX(button_box), label);
        
        gtk_button_set_child(GTK_BUTTON(category_button), button_box);
        gtk_box_append(GTK_BOX(main_box), category_button);
        
        // Crear popover para esta categoría
        GtkWidget *category_popover = gtk_popover_new();
        gtk_widget_set_parent(category_popover, category_button);
        gtk_popover_set_position(GTK_POPOVER(category_popover), GTK_POS_RIGHT);
        gtk_popover_set_autohide(GTK_POPOVER(category_popover), FALSE);
        gtk_popover_set_has_arrow(GTK_POPOVER(category_popover), FALSE);
        gtk_widget_add_css_class(category_popover, "menu-popover");
        
        GtkWidget *category_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_popover_set_child(GTK_POPOVER(category_popover), category_box);
        
        // Añadir aplicaciones
        for (GList *app_iter = app_list; app_iter != NULL; app_iter = app_iter->next) {
            GAppInfo *app_info = G_APP_INFO(app_iter->data);
            const gchar *app_id = g_app_info_get_id(app_info);
            if (!app_id) continue;
            
            // Crear botón de aplicación
            GtkWidget *app_button = gtk_button_new();
            gtk_button_set_has_frame(GTK_BUTTON(app_button), FALSE);
            gtk_widget_add_css_class(app_button, "menu-app-button");
            
            GtkWidget *app_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            
            // Icono de la aplicación
            GIcon *app_icon = g_app_info_get_icon(app_info);
            GtkWidget *icon_widget = app_icon ? 
                gtk_image_new_from_gicon(app_icon) : 
                gtk_image_new_from_icon_name("application-x-executable");
            gtk_image_set_pixel_size(GTK_IMAGE(icon_widget), 16);
            gtk_box_append(GTK_BOX(app_button_box), icon_widget);
            
            // Etiqueta de la aplicación
            GtkWidget *app_label = gtk_label_new(g_app_info_get_display_name(app_info));
            gtk_label_set_xalign(GTK_LABEL(app_label), 0.0);
            gtk_box_append(GTK_BOX(app_button_box), app_label);
            
            gtk_button_set_child(GTK_BUTTON(app_button), app_button_box);
            gtk_box_append(GTK_BOX(category_box), app_button);
            
            // Crear acción para lanzar aplicación
            LaunchData *launch_data = g_malloc(sizeof(LaunchData));
            launch_data->menu_button = self;
            launch_data->app_id = g_strdup(app_id);
            
            gchar *action_name = g_strdup_printf("launch-app-%s", app_id);
            // Limpiar caracteres especiales
            for (gchar *p = action_name; *p; p++) {
                if (!g_ascii_isalnum(*p) && *p != '-') *p = '-';
            }
            
            GSimpleAction *action = g_simple_action_new(action_name, NULL);
            g_signal_connect(action, "activate", G_CALLBACK(launch_app_callback), launch_data);
            g_action_map_add_action(G_ACTION_MAP(self->action_group), G_ACTION(action));
            
            g_object_set_data_full(G_OBJECT(action), "launch-data", launch_data, 
                                  (GDestroyNotify)launch_data_free);
            
            gchar *full_action_name = g_strdup_printf("app.%s", action_name);
            gtk_actionable_set_action_name(GTK_ACTIONABLE(app_button), full_action_name);
            
            g_free(action_name);
            g_free(full_action_name);
            g_object_unref(action);
        }
        
        // Almacenar información del submenu
        CategoryMenu *cat_menu = g_malloc(sizeof(CategoryMenu));
        cat_menu->popover = category_popover;
        cat_menu->category_name = g_strdup(category_key);
        cat_menu->menu_model = NULL;
        self->category_menus = g_slist_append(self->category_menus, cat_menu);
        
        // Datos del botón y eventos
        g_object_set_data_full(G_OBJECT(category_button), "category-name", 
                              g_strdup(category_key), g_free);
        
        GtkEventController *motion_controller = gtk_event_controller_motion_new();
        g_signal_connect(motion_controller, "enter", G_CALLBACK(on_category_enter), self);
        gtk_widget_add_controller(category_button, motion_controller);
        
        g_signal_connect(category_popover, "closed", G_CALLBACK(on_category_menu_closed), self);
    }
    
    // Limpieza
    g_list_free_full(app_infos, g_object_unref);
    g_hash_table_destroy(categories);

    // ✨ COMPUTER MENU - Añadir al FINAL del menú
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(main_box), separator);
    
    GtkWidget *computer_button = gtk_button_new();
    gtk_button_set_has_frame(GTK_BUTTON(computer_button), FALSE);
    gtk_widget_add_css_class(computer_button, "menu-category-button");
    
    GtkWidget *computer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *computer_icon = gtk_image_new_from_icon_name("computer");
    gtk_image_set_pixel_size(GTK_IMAGE(computer_icon), 16);
    gtk_box_append(GTK_BOX(computer_box), computer_icon);
    
    GtkWidget *computer_label = gtk_label_new("Computer");
    gtk_label_set_xalign(GTK_LABEL(computer_label), 0.0);
    gtk_box_append(GTK_BOX(computer_box), computer_label);
    
    gtk_button_set_child(GTK_BUTTON(computer_button), computer_box);
    gtk_box_append(GTK_BOX(main_box), computer_button);
    
    // Crear submenu Computer con eventos CORREGIDOS
    GtkWidget *computer_menu = create_computer_submenu(self);
    if (computer_menu) {
        gtk_widget_set_parent(computer_menu, computer_button);
        gtk_popover_set_position(GTK_POPOVER(computer_menu), GTK_POS_RIGHT);
        gtk_popover_set_autohide(GTK_POPOVER(computer_menu), FALSE);
        gtk_popover_set_has_arrow(GTK_POPOVER(computer_menu), FALSE);
        gtk_widget_add_css_class(computer_menu, "menu-popover");
        
        // Añadir a la lista de menús de categorías
        CategoryMenu *computer_cat_menu = g_malloc(sizeof(CategoryMenu));
        computer_cat_menu->popover = computer_menu;
        computer_cat_menu->category_name = g_strdup("Computer");
        computer_cat_menu->menu_model = NULL;
        self->category_menus = g_slist_append(self->category_menus, computer_cat_menu);
        
        // CRÍTICO: Datos del botón con g_strdup para consistencia
        g_object_set_data_full(G_OBJECT(computer_button), "category-name", 
                              g_strdup("Computer"), g_free);
        
        // CRÍTICO: Controlador de mouse para mostrar submenu
        GtkEventController *motion_controller = gtk_event_controller_motion_new();
        g_signal_connect(motion_controller, "enter", G_CALLBACK(on_category_enter), self);
        gtk_widget_add_controller(computer_button, motion_controller);
        
        // Callback para cerrar submenu
        g_signal_connect(computer_menu, "closed", G_CALLBACK(on_category_menu_closed), self);
    }

    // Conectar señales del menú principal
    g_signal_connect(self->menu_button, "clicked", G_CALLBACK(on_menu_button_clicked), self);
    g_signal_connect(self->main_menu, "closed", G_CALLBACK(on_main_menu_closed), self);
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

GtkWidget *app_menu_button_new(const gchar *icon_name, PanelConfig *config) {
    AppMenuButton *button = g_object_new(APP_TYPE_MENU_BUTTON, NULL);
        
    if (icon_name) {
        gtk_button_set_icon_name(GTK_BUTTON(button->menu_button), icon_name);
    }
    
    // Establecer configuración ANTES de construir menú
    button->config = config;
    
    // 🎯 AHORA construir el menú con la config disponible
    build_main_menu(button);
        
    return GTK_WIDGET(button);
}
