#include <functional>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>

#include "temail/client/imap.hpp"
#include "temail/client/response.hpp"
#include "temail/private/client/imap/list.hpp"
#include "temail/private/client/imap/response.hpp"

namespace temail::client::detail {

namespace {

const QRegularExpression LIST_REG{
  R"REGEX(\((?P<attrs>[^)]+)\) "(?P<parent>[^"]+)" "(?P<name>[^"]+)")REGEX"
}; /**< Regex to parse LIST response such as (\XXX \XXX) "XXX" "XXX" into
      (<attrs>) "<parent>" "<name>" */

QStringList
_parse_attrs(const QString& attrs_str)
{
  auto attrs = attrs_str.split(' ', Qt::SkipEmptyParts);

  for (auto& item : attrs) {
    if (item.front() == '\\') {
      item.erase(item.begin());
    }
  }

  return attrs;
}

}

void
imap_handle_list(
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

  auto list_resp = response::List{};

  for (const auto& [type, data] : resp->untagged()) {
    if (type != IMAP::Response::LIST) {
      qWarning() << "Failed to parse LIST response: Unexpected type." << type;
      continue;
    }

    auto parsed = LIST_REG.match(data);

    if (!parsed.hasMatch()) {
      qWarning() << "Failed to parse LIST response: Unexpect format." << data;
      continue;
    }

    list_resp.push_back({ parsed.captured("parent"),
                          parsed.captured("name"),
                          _parse_attrs(parsed.captured("attrs")) });
  }

  success_handler(QVariant::fromValue(list_resp));
}

}
