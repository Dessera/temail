#include <functional>
#include <qstring.h>
#include <qstringlist.h>
#include <qvariant.h>
#include <utility>

#include "temail/client/imap.hpp"
#include "temail/client/response.hpp"
#include "temail/private/client/imap/response.hpp"
#include "temail/private/client/imap/select.hpp"

namespace temail::client::detail {

namespace {

const QRegularExpression ATTRS_REG{
  R"REGEX(\((?P<attrs>[^)]+)\))REGEX"
}; /**< Regex to parse attrs such as (\XXX \XXX) into (<attrs>) */

const QRegularExpression SELECT_BRACKET_REG{
  R"REGEX(\[(?P<type>[A-Z-]+)( (\()?(?P<data>[^)]+)(\))?)?\])REGEX"
}; /**< Regex to parse bracket response such as [XXX XXX] XXX, [XXX] XXX or
      [XXX (\XXX \XXX)] XXX into [<type> <data>] XXX, [<type>] XXX or [<type>
      (<data>)] XXX */

// TODO: Duplicate.
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

// TODO: complexity
void
imap_handle_select(const detail::IMAPResponse& resp,
                   const IMAP::ErrorCallback& error_handler,
                   const IMAP::CommandCallback& success_handler)
{
  if (resp.tagged().size() != 1) {
    error_handler(IMAP::E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged()[0].first == IMAP::Response::NO) {
    error_handler(IMAP::E_REFERENCE, resp.tagged()[0].second);
    return;
  }

  if (resp.tagged()[0].first == IMAP::Response::BAD) {
    error_handler(IMAP::E_BADCOMMAND, resp.tagged()[0].second);
    return;
  }

  auto select_resp = response::Select{};

  if (auto parsed = SELECT_BRACKET_REG.match(resp.tagged()[0].second);
      parsed.hasMatch()) {
    select_resp.permission = parsed.captured("type");
  } else {
    qWarning()
      << "Failed to parse priority from SELECT response: Unexpected format."
      << resp.tagged()[0].second;
  }

  for (const auto& item : resp.untagged_trailing()) {
    if (item.first == IMAP::Response::EXISTS) {
      bool ok = false;
      auto exists = item.second.toULongLong(&ok);
      if (!ok) {
        qWarning() << "Failed to parse SELECT EXISTS response: Not a number.";
        continue;
      }
      select_resp.exists = exists;
      continue;
    }

    if (item.first == IMAP::Response::RECENT) {
      bool ok = false;
      auto recent = item.second.toULongLong(&ok);
      if (!ok) {
        qWarning() << "Failed to parse SELECT RECENT response: Not a number.";
        continue;
      }
      select_resp.recent = recent;
      continue;
    }
  }

  for (const auto& item : resp.untagged()) {
    if (auto parsed = ATTRS_REG.match(item.second);
        item.first == IMAP::Response::FLAGS && parsed.hasMatch()) {
      select_resp.flags = _parse_attrs(parsed.captured("attrs"));
    }

    if (auto parsed = SELECT_BRACKET_REG.match(item.second);
        item.first == IMAP::Response::OK && parsed.hasMatch()) {
      if (parsed.captured("type") == "UNSEEN" && parsed.hasCaptured("data")) {
        bool ok = false;
        auto unseen = parsed.captured("data").toULongLong(&ok);
        if (!ok) {
          qWarning() << "Failed to parse SELECT UNSEEN response: Not a number.";
          continue;
        }
        select_resp.unseen = unseen;
        continue;
      }

      if (parsed.captured("type") == "UIDVALIDITY" &&
          parsed.hasCaptured("data")) {
        bool ok = false;
        auto uidvalidity = parsed.captured("data").toULongLong(&ok);
        if (!ok) {
          qWarning()
            << "Failed to parse SELECT UIDVALIDITY response: Not a number.";
          continue;
        }
        select_resp.uidvalidity = uidvalidity;
        continue;
      }

      if (parsed.captured("type") == "PERMANENTFLAGS" &&
          parsed.hasCaptured("data")) {
        select_resp.permanent_flags = _parse_attrs(parsed.captured("data"));
      }
    }
  }

  success_handler(QVariant::fromValue(std::move(select_resp)));
}

}
