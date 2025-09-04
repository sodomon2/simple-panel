#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct {
    // Global settings
    gchar *edge;
    gint panel_size;
    
    // Menu settings  
    gboolean menu_enable;
    gchar *menu_icon;
    
    // System Tray settings
    gboolean systray_enable;
    gint systray_icon_size;
    
    // Clock settings
    gboolean clock_enable;
    gint clock_size;
    gchar *clock_weight;
    gchar *clock_color;
} PanelConfig;

// Functions
PanelConfig *panel_config_new(void);
void panel_config_free(PanelConfig *config);
gboolean panel_config_load(PanelConfig *config, const gchar *config_path);
gboolean panel_config_save(PanelConfig *config, const gchar *config_path);
void panel_config_create_default(const gchar *config_path);
gchar *panel_config_get_default_path(void);

G_END_DECLS

#endif // CONFIG_H
