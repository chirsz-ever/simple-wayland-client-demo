project(
  'wayland-client-demo',
  'c',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'c_std=c99'],
)

dependencies = [
  dependency('wayland-client')
]

exe = executable(
  'demo',
  'main.c',
  install : true,
  dependencies : dependencies,
)

test('basic', exe)
