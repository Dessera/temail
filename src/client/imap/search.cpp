#include <functional>
#include <qstring.h>
#include <qvariant.h>
#include <utility>

#include "temail/client/imap.hpp"
#include "temail/private/client/imap/response.hpp"
#include "temail/private/client/imap/search.hpp"

namespace temail::client::detail {

void
imap_handle_search(
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

  if (resp->untagged().size() != 1) {
    error_handler(IMAP::E_UNEXPECTED, "Unexpected untagged response");
    return;
  }

  auto search_resp = QList<std::size_t>{};
  for (const auto& item :
       resp->untagged()[0].second.split(' ', Qt::SkipEmptyParts)) {
    bool ok = false;
    auto index = item.toULongLong(&ok);
    if (!ok) {
      qWarning() << "Failed to parse SEARCH response: Not a number.";
      continue;
    }
    search_resp.push_back(index);
  }

  success_handler(QVariant::fromValue(std::move(search_resp)));
}

}
