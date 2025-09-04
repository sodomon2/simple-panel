#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

// Forward declaration
typedef struct _PanelConfig PanelConfig;

#define APP_TYPE_MENU_BUTTON (app_menu_button_get_type())
G_DECLARE_FINAL_TYPE(AppMenuButton, app_menu_button, APP, MENU_BUTTON, GtkBox)

GtkWidget *app_menu_button_new(const gchar *icon_name, PanelConfig *config);

G_END_DECLS
