lib_src = files(
  'tag.cpp',
)

lib_src += qt.compile_moc(headers: lib_qt_moc_src, dependencies: lib_deps)
# lib_src += qt.compile_resources(sources: lib_qt_res_src)

subdir('client')