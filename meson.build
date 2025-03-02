project(
  'multiwatch',
  'c',
  default_options: [
    'buildtype=debugoptimized',
    'warning_level=3',
    'c_std=gnu99',
  ],
  version: '1.0.0',
  license: 'BSD-3-Clause',
)

compiler = meson.get_compiler('c')

conf_data = configuration_data()
conf_data.set_quoted('PACKAGE_VERSION', meson.project_version())
conf_data.set_quoted('PACKAGE_NAME', meson.project_name())

dep_glib = dependency('glib-2.0', version: '>=2.16')

# find libev manually
dep_ev = compiler.find_library(
  'ev',
  has_headers: 'ev.h',
)
if not compiler.has_function(
  'ev_time',
  prefix: '#include <ev.h>',
  dependencies: dep_ev,
)
  error('Missing ev_time() in libev')
endif

warn_c_args = [
  '-Wshadow',
  '-W',
  '-pedantic',
]
if get_option('extra-warnings')
  warn_c_args += [
    '-Wmissing-declarations',
    '-Wdeclaration-after-statement',
    '-Wcast-align',
    '-Wsign-compare',
    '-Wnested-externs',
    '-Wpointer-arith',
    '-Wmissing-prototypes',
    '-Wno-pointer-sign',
    '-Wformat',
    '-Wformat-security',
    '-D_FORTIFY_SOURCE=2',
    '-fstack-protector',
    '--param=ssp-buffer-size=4',
  ]
endif

add_project_arguments(
  compiler.get_supported_arguments(warn_c_args),
  language: 'c'
)

configure_file(output: 'config.h', configuration: conf_data)
add_global_arguments('-DHAVE_CONFIG_H', language : 'c')

bin_worker = executable(
  'multiwatch',
  'multiwatch.c',
  install: true,
  dependencies: [
    dep_glib,
    dep_ev,
  ],
)

install_man(
  'multiwatch.1',
)

summary(
  {
    'Extra warnings': get_option('extra-warnings')
  },
  section: 'Features',
)
