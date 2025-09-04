project('simple-panel', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11', 'warning_level=2'])

# Importar gnome module para recursos
gnome = import('gnome')

# Dependencias
gtk_dep = dependency('gtk4', version: '>= 4.8')
gio_unix_dep = dependency('gio-unix-2.0')
layershell_dep = dependency('gtk4-layer-shell-0')

# Compilar recursos XML del status service
status_service_resources = gnome.compile_resources(
  'status-service-resources',
  'data/status_service/status_service.gresource.xml',
  source_dir: 'data/status_service'
)

# El ejecutable principal de nuestro panel
executable('simple-panel',
  'src/main.c',
  'src/panel.c',
  'src/plugins/app_menu_button.c',
  'src/plugins/clock_widget.c',
  'src/plugins/launcher_widget.c',
  'src/plugins/systray_widget.c',
  'src/config.c',
  status_service_resources,
  dependencies : [gtk_dep, gio_unix_dep, layershell_dep],
  install : true)
