project(
  'wayland-client-demo',
  'cpp',
  version : '0.1',
  meson_version : '>= 1.3.0',
  default_options : ['warning_level=3', 'cpp_std=c++20'],
)

dependencies = [
  dependency('wayland-client')
]

exe = executable(
  'demo',
  'main.cpp',
  install : true,
  dependencies : dependencies,
)

test('basic', exe)
