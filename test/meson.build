test_deps = [temail_dep]
test_args = [
  '-Wall',
  '-Wextra',
  '-Wno-pedantic',
  '-Werror',
  '-DTEMAIL_TEST_IMAP_HOST="' + test_imap_host + '"',
  '-DTEMAIL_TEST_IMAP_PORT=' + test_imap_port.to_string(),
  '-DTEMAIL_TEST_IMAP_USE_SSL=' + test_imap_use_ssl.to_string(),
  '-DTEMAIL_TEST_IMAP_USERNAME="' + test_imap_username + '"',
  '-DTEMAIL_TEST_IMAP_PASSWORD="' + test_imap_password + '"',
]

subdir('client')