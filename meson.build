project('simple-panel', 'c',
  version : '0.1.0',
  default_options : ['c_std=c11', 'warning_level=2'])

# Importar gnome module para recursos
gnome = import('gnome')

# Dependencias
gtk_dep = dependency('gtk4', version: '>= 4.8')
gio_unix_dep = dependency('gio-unix-2.0')
layershell_dep = dependency('gtk4-layer-shell-0')
wayland_scanner = dependency('wayland-scanner', native: true)
wayland_scanner_prog = find_program(wayland_scanner.get_variable('wayland_scanner'))

# Compilar recursos del panel principal
simple_panel_resources = gnome.compile_resources(
  'simple-panel-resources',
  'data/simple_panel.gresource.xml',
  source_dir: 'data'
)

# wlr-protocols para tasklist
wlr_protocols = dependency('wlr-protocols', version: '>= 0.1', required: true)
wlr_protocols_dir = wlr_protocols.get_variable('pkgdatadir')

# Generate protocol headers para tasklist
wlr_foreign_toplevel_xml = join_paths(
  wlr_protocols_dir,
  'unstable/wlr-foreign-toplevel-management-unstable-v1.xml'
)

wlr_foreign_toplevel_client_h = custom_target(
  'wlr-foreign-toplevel-client-protocol.h',
  input: wlr_foreign_toplevel_xml,
  output: 'wlr-foreign-toplevel-management-unstable-v1-client-protocol.h',
  command: [wayland_scanner_prog, 'client-header', '@INPUT@', '@OUTPUT@']
)

wlr_foreign_toplevel_private_c = custom_target(
  'wlr-foreign-toplevel-private-code.c',
  input: wlr_foreign_toplevel_xml,
  output: 'wlr-foreign-toplevel-management-unstable-v1-protocol.c',
  command: [wayland_scanner_prog, 'private-code', '@INPUT@', '@OUTPUT@']
)

tasklist_sources = [wlr_foreign_toplevel_client_h, wlr_foreign_toplevel_private_c]
tasklist_deps = [dependency('wayland-client')]

# El ejecutable principal de nuestro panel
executable('simple-panel',
  'src/main.c',
  'src/panel.c',
  'src/plugins/app_menu_button.c',
  'src/plugins/clock_widget.c',
  'src/plugins/launcher_widget.c',
  'src/plugins/systray_widget.c',
  'src/plugins/tasklist_widget.c',
  'src/plugins/showdesktop_widget.c',
  'src/plugins/ram_monitor_widget.c',
  'src/plugins/cpu_monitor_widget.c',
  'src/plugins/net_monitor_widget.c',
  'src/config.c',
  simple_panel_resources,
  tasklist_sources,
  dependencies : [gtk_dep, gio_unix_dep, layershell_dep, tasklist_deps],
  c_args: ['-DHAVE_WLR_PROTOCOLS'],
  install : true)