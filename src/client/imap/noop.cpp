#include <functional>
#include <qstring.h>
#include <qvariant.h>

#include "temail/client/imap.hpp"
#include "temail/client/response.hpp"
#include "temail/private/client/imap/noop.hpp"
#include "temail/private/client/imap/response.hpp"

namespace temail::client::detail {

void
imap_handle_noop(const detail::IMAPResponse& resp,
                 const IMAP::ErrorCallback& error_handler,
                 const IMAP::CommandCallback& success_handler)
{
  if (resp.tagged().size() != 1) {
    error_handler(IMAP::E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged()[0].first != IMAP::Response::OK) {
    error_handler(IMAP::E_BADCOMMAND, resp.tagged()[0].second);
    return;
  }

  success_handler(QVariant::fromValue(response::Noop{}));
}

}
