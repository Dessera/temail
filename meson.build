project(
  'temail',
  'cpp',
  version: '0.1.0',
  default_options: ['warning_level=3', 'cpp_std=gnu++17'],
)

qt = import('qt6')
qt_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'Network'])

lib_deps = [qt_dep]

subdir('include')
subdir('src')

lib = library(
  'temail',
  lib_src,
  include_directories: lib_inc,
  dependencies: lib_deps,
  gnu_symbol_visibility: 'hidden',
  cpp_args: [
    '-DBUILDING_TEMAIL',
    '-Wall',
    '-Wextra',
    '-Wno-pedantic',
    '-Werror',
  ],
  install: true,
)

temail_dep = declare_dependency(
  include_directories: lib_inc,
  link_with: lib,
  dependencies: [qt_dep],
)

subdir('bin')

pkg_mod = import('pkgconfig')

pkg_mod.generate(
  name: 'temail',
  filebase: 'temail',
  description: 'Email client written in Qt.',
  libraries: [lib],
  version: '0.1.0',
)