#include <functional>
#include <qstring.h>
#include <qvariant.h>

#include "temail/client/imap.hpp"
#include "temail/private/client/imap/fetch.hpp"
#include "temail/private/client/imap/response.hpp"

namespace temail::client::detail {

void
imap_handle_fetch(
  IMAPResponse* resp,
  const std::function<void(IMAP::ErrorType, const QString&)>& error_handler,
  const std::function<void(const QVariant&)>& success_handler)
{
  if (resp->tagged().size() != 1) {
    error_handler(IMAP::E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp->tagged()[0].first == IMAP::Response::NO) {
    error_handler(IMAP::E_REFERENCE, resp->tagged()[0].second);
    return;
  }

  if (resp->tagged()[0].first == IMAP::Response::BAD) {
    error_handler(IMAP::E_BADCOMMAND, resp->tagged()[0].second);
    return;
  }

  success_handler(QVariant::fromValue(1));
}

}
