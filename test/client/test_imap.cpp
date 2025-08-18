#include <qtest.h>
#include <qtestcase.h>
#include <temail/client/base.hpp>
#include <temail/client/response.hpp>

#include "temail/client/request.hpp"
#include "test_imap.hpp"

void
IMAPTest::test_interface() // NOLINT
{
  _client->connect_to_host(TEMAIL_TEST_IMAP_HOST,
                           TEMAIL_TEST_IMAP_PORT,
                           TEMAIL_TEST_IMAP_USE_SSL ? client::Base::USE_SSL
                                                    : client::Base::NO_SSL);
  QVERIFY(_client->wait_for_connected());

  _client->login(TEMAIL_TEST_IMAP_USERNAME, TEMAIL_TEST_IMAP_PASSWORD);
  QVERIFY(_client->wait_for_ready_read());
  QVERIFY(_client->read().canConvert<client::response::Login>());

  _client->list("\"\"", "*");
  QVERIFY(_client->wait_for_ready_read());
  QVERIFY(_client->read().canConvert<client::response::List>());

  _client->select("INBOX");
  QVERIFY(_client->wait_for_ready_read());
  QVERIFY(_client->read().canConvert<client::response::Select>());

  _client->noop();
  QVERIFY(_client->wait_for_ready_read());
  QVERIFY(_client->read().canConvert<client::response::Noop>());

  _client->search(client::request::Search::ALL);
  QVERIFY(_client->wait_for_ready_read());
  QVERIFY(_client->read().canConvert<client::response::Search>());

  _client->fetch(1,
                 client::request::Fetch::TEXT | client::request::Fetch::MIME);
  QVERIFY(_client->wait_for_ready_read());

  _client->logout();
  QVERIFY(_client->wait_for_disconnected());
}

QTEST_MAIN(IMAPTest)
