project(
  'temail',
  'cpp',
  version: '0.1.0',
  default_options: ['warning_level=3', 'cpp_std=gnu++17'],
)

build_test = get_option('build_test')
test_imap_host = get_option('test_imap_host')
test_imap_port = get_option('test_imap_port')
test_imap_use_ssl = get_option('test_imap_use_ssl')
test_imap_username = get_option('test_imap_username')
test_imap_password = get_option('test_imap_password')

qt = import('qt6')
qt_dep = dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'Network', 'Test'])

lib_deps = [qt_dep]
lib_args = [
  '-DBUILDING_TEMAIL',
  '-Wall',
  '-Wextra',
  '-Wno-pedantic',
  '-Werror',
]

subdir('include')
subdir('src')

lib = library(
  'temail',
  lib_src,
  gnu_symbol_visibility: 'hidden',
  include_directories: lib_inc,
  dependencies: lib_deps,
  cpp_args: lib_args,
  install: true,
)

temail_dep = declare_dependency(
  include_directories: lib_inc,
  link_with: lib,
  dependencies: lib_deps,
)

if build_test.enabled()
  subdir('test')
endif

pkg_mod = import('pkgconfig')

pkg_mod.generate(
  name: 'temail',
  filebase: 'temail',
  description: 'Email client written in Qt.',
  libraries: [lib],
  version: '0.1.0',
)