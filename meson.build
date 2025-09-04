project('simple-panel', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11', 'warning_level=2'])

# Dependencia para GTK4
gtk_dep = dependency('gtk4', version: '>= 4.8')
gio_unix_dep = dependency('gio-unix-2.0')

# Dependencia para el protocolo wlr-layer-shell a través de su librería helper.
# Esto nos da las funciones como gtk_layer_init_for_window()
layershell_dep = dependency('gtk4-layer-shell-0', version: '>= 0.1')

# El ejecutable principal de nuestro panel
executable('simple-panel',
  'src/main.c',
  'src/panel.c',
  'src/plugins/app_menu_button.c',
  'src/plugins/clock_widget.c',
  'src/plugins/launcher_widget.c',
  'src/config.c',
  dependencies : [gtk_dep, gio_unix_dep, layershell_dep],
  install : true)
