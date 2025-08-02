#include <qapplication.h>
#include <qlogging.h>

#include "temail/client/imap.hpp"
#include "temail/client/request.hpp"

int
main(int argc, char** argv)
{
  using temail::client::IMAP;

  auto app = QApplication{ argc, argv };

  auto client = IMAP{};

  client.connect_to_host("imap.qq.com");
  if (!client.wait_for_connected()) {
    qDebug() << client.error_string();
    return 1;
  }

  client.login("dessera@qq.com", "wyjarpdqwvcqjhdd");
  if (!client.wait_for_ready_read()) {
    qDebug() << client.error_string();
    return 1;
  }

  qDebug() << client.read();

  client.select("INBOX");
  if (!client.wait_for_ready_read()) {
    qDebug() << client.error_string();
    return 1;
  }

  qDebug() << client.read();

  client.fetch(1, temail::client::request::Fetch::TEXT);
  if (!client.wait_for_ready_read()) {
    qDebug() << client.error_string();
    return 1;
  }

  qDebug() << client.read();
}
