project(
  'wayland-client-demo',
  'c',
  version : '0.1',
  meson_version : '>= 1.8.0',
  default_options : ['warning_level=3', 'c_std=c99'],
)

wl_mod = import('wayland')
xdg_shell_xml = wl_mod.find_protocol('xdg-shell')
xdg_shell = wl_mod.scan_xml(xdg_shell_xml)

dependencies = [
  dependency('wayland-client'),
  dependency('wayland-cursor'),
]

executable(
  'demo',
  'main.c',
  xdg_shell,
  dependencies : dependencies,
)
