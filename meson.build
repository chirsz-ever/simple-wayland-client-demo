project(
  'wayland-client-demo',
  'c',
  version : '0.1',
  meson_version : '>= 1.8.0',
  default_options : ['warning_level=3', 'c_std=c99'],
)

compiler = meson.get_compiler('c')
if compiler.has_function('memfd_create', prefix : '#define _GNU_SOURCE\n#include <sys/mman.h>')
  add_project_arguments(['-DHAS_MEMFD', '-D_GNU_SOURCE'], language : 'c')
endif

add_project_arguments('-D_DEFAULT_SOURCE', language : 'c')

wl_mod = import('wayland')

dependencies = [
  dependency('wayland-client'),
  dependency('wayland-cursor'),
]

executable(
  'demo',
  'main.c',
  wl_mod.scan_xml(wl_mod.find_protocol('xdg-shell')),
  wl_mod.scan_xml(wl_mod.find_protocol('cursor-shape', state: 'staging', version: 1)),
  wl_mod.scan_xml(wl_mod.find_protocol('tablet', version: 2)),
  wl_mod.scan_xml(wl_mod.find_protocol('xdg-decoration', state: 'unstable', version: 1)),
  dependencies : dependencies,
  link_args: '-lm',
)
