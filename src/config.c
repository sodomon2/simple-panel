#include "config.h"
#include <stdio.h>
#include <glib/gstdio.h>

// Crear nueva estructura de configuración vacía
PanelConfig *panel_config_new(void) {
    PanelConfig *config = g_malloc0(sizeof(PanelConfig));
    
    // Inicializar con valores NULL/0 - se cargarán desde archivo
    config->edge = NULL;
    config->panel_size = 0;
    
    config->menu_enable = FALSE;
    config->menu_icon = NULL;
    
    config->clock_enable = FALSE;
    config->clock_size = 0;
    config->clock_weight = NULL;
    config->clock_color = NULL;
    
    return config;
}

// Liberar memoria de la configuración
void panel_config_free(PanelConfig *config) {
    if (!config) return;
    
    g_free(config->edge);
    g_free(config->menu_icon);
    g_free(config->clock_weight);
    g_free(config->clock_color);
    g_free(config);
}

// Funciones helper para eliminar redundancia
static void load_string_key(GKeyFile *key_file, const gchar *group, const gchar *key, gchar **target) {
    if (g_key_file_has_key(key_file, group, key, NULL)) {
        g_free(*target);
        *target = g_key_file_get_string(key_file, group, key, NULL);
    }
}

static void load_int_key(GKeyFile *key_file, const gchar *group, const gchar *key, gint *target) {
    if (g_key_file_has_key(key_file, group, key, NULL)) {
        *target = g_key_file_get_integer(key_file, group, key, NULL);
    }
}

static void load_bool_key(GKeyFile *key_file, const gchar *group, const gchar *key, gboolean *target) {
    if (g_key_file_has_key(key_file, group, key, NULL)) {
        *target = g_key_file_get_boolean(key_file, group, key, NULL);
    }
}

// Aplicar valores por defecto para claves faltantes
static void apply_default_values(PanelConfig *config) {
    if (!config->edge) config->edge = g_strdup("bottom");
    if (config->panel_size <= 0) config->panel_size = 32;
    
    if (!config->menu_icon) config->menu_icon = g_strdup("start-here-symbolic");
    
    if (config->clock_size <= 0) config->clock_size = 13;
    if (!config->clock_weight) config->clock_weight = g_strdup("normal");
    if (!config->clock_color) config->clock_color = g_strdup("white");
}

// Cargar configuración desde archivo INI
gboolean panel_config_load(PanelConfig *config, const gchar *config_path) {
    GKeyFile *key_file;
    GError *error = NULL;
    
    key_file = g_key_file_new();
    
    if (!g_key_file_load_from_file(key_file, config_path, G_KEY_FILE_NONE, &error)) {
        g_warning("No se pudo cargar el archivo de configuración: %s", error->message);
        g_error_free(error);
        g_key_file_free(key_file);
        // Aplicar valores por defecto y continuar
        apply_default_values(config);
        return FALSE;
    }
    
    // Cargar configuración global
    if (g_key_file_has_group(key_file, "global")) {
        load_string_key(key_file, "global", "edge", &config->edge);
        load_int_key(key_file, "global", "size", &config->panel_size);
    }
    
    // Cargar configuración del menú
    if (g_key_file_has_group(key_file, "menu-classic")) {
        load_bool_key(key_file, "menu-classic", "enable", &config->menu_enable);
        load_string_key(key_file, "menu-classic", "icon", &config->menu_icon);
    }
    
    // Cargar configuración del reloj
    if (g_key_file_has_group(key_file, "clock")) {
        load_bool_key(key_file, "clock", "enable", &config->clock_enable);
        load_int_key(key_file, "clock", "size", &config->clock_size);
        load_string_key(key_file, "clock", "weight", &config->clock_weight);
        load_string_key(key_file, "clock", "color", &config->clock_color);
    }
    
    // Aplicar valores por defecto para cualquier clave que falte
    apply_default_values(config);
    
    g_key_file_free(key_file);
    return TRUE;
}

// Guardar configuración en archivo INI
gboolean panel_config_save(PanelConfig *config, const gchar *config_path) {
    GKeyFile *key_file;
    GError *error = NULL;
    gboolean success = TRUE;
    
    key_file = g_key_file_new();
    
    // Configuración global
    g_key_file_set_string(key_file, "global", "edge", config->edge);
    g_key_file_set_integer(key_file, "global", "size", config->panel_size);
    
    // Configuración del menú
    g_key_file_set_boolean(key_file, "menu-classic", "enable", config->menu_enable);
    g_key_file_set_string(key_file, "menu-classic", "icon", config->menu_icon);
    
    // Configuración del reloj
    g_key_file_set_boolean(key_file, "clock", "enable", config->clock_enable);
    g_key_file_set_integer(key_file, "clock", "size", config->clock_size);
    g_key_file_set_string(key_file, "clock", "weight", config->clock_weight);
    g_key_file_set_string(key_file, "clock", "color", config->clock_color);
    
    // Crear directorio padre si no existe
    gchar *dir = g_path_get_dirname(config_path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    
    // Guardar archivo
    if (!g_key_file_save_to_file(key_file, config_path, &error)) {
        g_warning("No se pudo guardar el archivo de configuración: %s", error->message);
        g_error_free(error);
        success = FALSE;
    }
    
    g_key_file_free(key_file);
    return success;
}

// Buscar archivo de configuración de ejemplo en directorios conocidos
static gchar *find_example_config(void) {
    // Posibles ubicaciones del archivo de ejemplo
    const gchar *possible_paths[] = {
        "../data/config.ini",                   // Directorio padre
        "/usr/share/simple-panel/config.ini", // Instalación del sistema
        NULL
    };
    
    for (int i = 0; possible_paths[i] != NULL; i++) {
        if (g_file_test(possible_paths[i], G_FILE_TEST_EXISTS)) {
            return g_strdup(possible_paths[i]);
        }
    }
    
    return NULL;
}

// Crear archivo de configuración por defecto copiando el ejemplo
void panel_config_create_default(const gchar *config_path) {
    gchar *example_config = find_example_config();
    
    if (example_config) {
        // Copiar archivo de ejemplo
        gchar *contents = NULL;
        gsize length = 0;
        GError *error = NULL;
        
        if (g_file_get_contents(example_config, &contents, &length, &error)) {
            // Crear directorio padre si no existe
            gchar *dir = g_path_get_dirname(config_path);
            g_mkdir_with_parents(dir, 0755);
            g_free(dir);
            
            // Escribir contenido al archivo de configuración del usuario
            if (g_file_set_contents(config_path, contents, length, &error)) {
                g_print("Configuración copiada desde: %s → %s\n", example_config, config_path);
            } else {
                g_warning("Error al crear configuración: %s", error->message);
                g_error_free(error);
            }
            g_free(contents);
        } else {
            g_warning("Error al leer configuración de ejemplo: %s", error->message);
            g_error_free(error);
        }
        g_free(example_config);
    } else {
        g_warning("No se encontró archivo config.ini de ejemplo, creando configuración mínima");
        // Fallback: crear configuración mínima si no hay archivo de ejemplo
        gchar *minimal_config = 
            "[global]\n"
            "edge=bottom\n"
            "size=32\n\n"
            "[menu-classic]\n"
            "enable=true\n"
            "icon=start-here-symbolic\n\n"
            "[clock]\n"
            "enable=true\n"
            "size=13\n"
            "weight=normal\n"
            "color=white\n\n"
            "[launchers]\n"
            "enable=false\n";
            
        gchar *dir = g_path_get_dirname(config_path);
        g_mkdir_with_parents(dir, 0755);
        g_free(dir);
        
        GError *error = NULL;
        if (!g_file_set_contents(config_path, minimal_config, -1, &error)) {
            g_warning("Error al crear configuración mínima: %s", error->message);
            g_error_free(error);
        } else {
            g_print("Configuración mínima creada en: %s\n", config_path);
        }
    }
}

// Obtener ruta por defecto del archivo de configuración
gchar *panel_config_get_default_path(void) {
    const gchar *config_dir = g_get_user_config_dir();
    return g_build_filename(config_dir, "simple-panel", "config.ini", NULL);
}
