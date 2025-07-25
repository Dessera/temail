#include <qapplication.h>
#include <qlogging.h>

#include "temail/clients/imap.hpp"

int
main(int argc, char** argv)
{
  using temail::clients::IMAP;

  auto app = QApplication{ argc, argv };

  auto client = IMAP{};

  client.connect_to_host("imap.qq.com");
  if (!client.wait_for_connected()) {
    qDebug() << client.error_string();
    return 1;
  }

  client.disconnect_from_host();
  if (!client.wait_for_disconnected()) {
    qDebug() << client.error_string();
    return 1;
  }
}
