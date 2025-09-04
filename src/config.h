#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _PanelConfig {
    // Global settings
    gchar *edge;
    gint panel_size;
    
    // System commands (Computer menu)
    gchar *lock_cmd;
    gchar *suspend_cmd;
    gchar *poweroff_cmd;
    gchar *reboot_cmd;
    gchar *logout_cmd;
    
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
    
    // Show Desktop settings
    gboolean showdesktop_enable;
    
    // System Monitor widgets
    gboolean ram_monitor_enable;
    gboolean cpu_monitor_enable;
    gboolean net_monitor_enable;
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
