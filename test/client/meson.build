test_imap_src = files('test_imap.cpp')
test_imap_src += qt.compile_moc(
  headers: files('test_imap.hpp'),
  dependencies: test_deps,
)

test_imap = executable(
  'test_imap',
  test_imap_src,
  dependencies: test_deps,
  cpp_args: test_args,
)

test('test_imap', test_imap)