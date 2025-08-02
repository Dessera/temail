#include <functional>
#include <qstring.h>
#include <qvariant.h>

#include "temail/client/imap.hpp"
#include "temail/private/client/imap/logout.hpp"
#include "temail/private/client/imap/response.hpp"

namespace temail::client::detail {

void
imap_handle_logout(
  IMAPResponse* resp,
  const std::function<void(IMAP::ErrorType, const QString&)>& error_handler,
  const std::function<void(const QVariant&)>& /*success_handler*/)
{
  if (resp->tagged().size() != 1) {
    error_handler(IMAP::E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp->tagged()[0].first != IMAP::Response::OK) {
    error_handler(IMAP::E_BADCOMMAND, resp->tagged()[0].second);
    return;
  }
}

}
