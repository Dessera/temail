#include <cstddef>
#include <cstdint>
#include <qanystringview.h>
#include <qbytearray.h>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qmetaobject.h>
#include <qpair.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvariant.h>
#include <utility>

#include "temail/client/imap.hpp"
#include "temail/client/request.hpp"
#include "temail/client/response.hpp"
#include "temail/common.hpp"
#include "temail/tag.hpp"

namespace temail::client {

namespace {

/**
 * @brief IMAP4 response parser.
 *
 */
class IMAPResponse
{
private:
  inline static QRegularExpression TAGGED_REG{
    "(?P<tag>[A-Z]\\d+) (?P<type>[A-Z]+) (?P<data>.*)"
  }; /**< Regex to parse tagged response. */

  inline static QRegularExpression UNTAGGED_REG{
    "\\* (?P<type>[A-Z-]+)( (?P<data>.*))?"
  }; /**< Regex to parse untagged response. */

  inline static QRegularExpression UNTAGGED_TRAILING_REG{
    "\\* (?P<data>.*) (?P<type>[A-Z-]+)"
  }; /**< Regex to parse untagged response. */

  QSslSocket* _sock;

public:
  QList<QPair<IMAP::Response, QString>> tagged;            // NOLINT
  QList<QPair<IMAP::Response, QString>> untagged;          // NOLINT
  QList<QPair<IMAP::Response, QString>> untagged_trailing; // NOLINT
  bool continuation;                                       // NOLINT

  IMAPResponse(QSslSocket* sock, const QString& tag)
    : _sock{ sock }
  {
    while (true) {
      auto buffer = _sock->readLine();
      if (buffer.isEmpty()) {
        break;
      }

      if (buffer.startsWith('*')) {
        _handle_untagged(buffer.trimmed());
        continue;
      }

      if (buffer.startsWith(tag.toLocal8Bit())) {
        _handle_tagged(buffer.trimmed());
        continue;
      }

      if (buffer.startsWith("+")) {
        continuation = true;
        continue;
      }

      qWarning() << "IMAP4 CLient: Unhandled response line: " << buffer;
    }
  }

private:
  void _handle_tagged(const QString& buffer)
  {
    if (auto parsed = TAGGED_REG.match(buffer); parsed.hasMatch()) {
      tagged.emplace_back(common::enum_value<IMAP::Response>(
                            parsed.captured("type").toLocal8Bit().constData()),
                          parsed.captured("data"));
      return;
    }

    qWarning() << "IMAP4 CLient: Unhandled response line: " << buffer;
  }

  void _handle_untagged(const QString& buffer)
  {
    if (auto parsed = UNTAGGED_REG.match(buffer); parsed.hasMatch()) {
      untagged.emplace_back(
        common::enum_value<IMAP::Response>(
          parsed.captured("type").toLocal8Bit().constData()),
        parsed.captured("data"));
      return;
    }

    if (auto parsed = UNTAGGED_TRAILING_REG.match(buffer); parsed.hasMatch()) {
      untagged_trailing.emplace_back(
        common::enum_value<IMAP::Response>(
          parsed.captured("type").toLocal8Bit().constData()),
        parsed.captured("data"));
      return;
    }

    qWarning() << "IMAP4 CLient: Unhandled response line: " << buffer;
  }
};

}

IMAP::IMAP(QObject* parent)
  : QObject{ parent }
  , _sock{ new QSslSocket{ this } }
{
  connect(_sock, &QSslSocket::connected, this, &IMAP::_on_connected);
  connect(_sock, &QSslSocket::disconnected, this, &IMAP::_on_disconnected);
}

IMAP::~IMAP()
{
  if (is_connected()) {
    logout();
    wait_for_disconnected();
  }
}

void
IMAP::connect_to_host(const QString& url, uint16_t port, SslOption ssl)
{
  if (is_connected()) {
    return;
  }

  if (port == 0) {
    port = ssl == USE_SSL ? PORT_USE_SSL : PORT_NO_SSL;
  }

  qDebug("IMAP4 Client: Try to connect to host %s:%d %s.",
         qPrintable(url),
         port,
         ssl == USE_SSL ? "with SSL" : "no SSL");

  if (ssl == USE_SSL) {
    _sock->connectToHostEncrypted(url, port);
  } else {
    _sock->connectToHost(url, port);
  }
}

void
IMAP::connect_to_host(const QString& url, SslOption ssl)
{
  connect_to_host(url, 0, ssl);
}

void
IMAP::disconnect_from_host()
{
  if (_status == S_DISCONNECT) {
    return;
  }

  qDebug() << "IMAP4 Client: Try to disconnect from host.";

  _sock->disconnectFromHost();
}

bool
IMAP::wait_for_connected(int msecs)
{
  if (is_connected()) {
    return true;
  }

  _wait_for_event(msecs, &IMAP::connected);

  return is_connected();
}

bool
IMAP::wait_for_disconnected(int msecs)
{
  if (is_disconnected()) {
    return true;
  }

  _wait_for_event(msecs, &IMAP::disconnected);

  return is_disconnected();
}

bool
IMAP::wait_for_ready_read(int msecs)
{
  _wait_for_event(msecs, &IMAP::ready_read);

  return _error == E_UNKNOWN;
}

QString
IMAP::error_string() const
{
  if (_error == E_TCPINTERNAL) {
    return _sock->errorString();
  }

  return _estr;
}

void
IMAP::login(const QString& username, const QString& password)
{
  if (_status == S_AUTHENTICATE) {
    return;
  }

  _send_command(Command::LOGIN,
                QString{ "LOGIN %1 %2" }.arg(username).arg(password));
}

void
IMAP::logout()
{
  if (_status == S_CONNECT) {
    disconnect_from_host();
    return;
  }

  _send_command(Command::LOGOUT, "LOGOUT");
}

void
IMAP::list(const QString& path, const QString& pattern)
{
  _send_command(Command::LIST, QString{ "LIST %2 %3" }.arg(path).arg(pattern));
}

void
IMAP::select(const QString& path)
{
  _send_command(Command::SELECT, QString{ "SELECT %2" }.arg(path));
}

void
IMAP::noop()
{
  _send_command(Command::NOOP, "NOOP");
}

void
IMAP::search(request::Search::Criteria criteria)
{
  _send_command(Command::SEARCH,
                QString{ "SEARCH %1" }.arg(common::enum_name(criteria)));
}

QVariant
IMAP::read()
{
  if (_queue.size() == 0) {
    qWarning() << "Failed to read from IMAP client: No response in queue.";
    return {};
  }

  return _queue.dequeue();
}

void
IMAP::_send_command(Command type, QAnyStringView cmd)
{
  if (_status == S_DISCONNECT) {
    _trig_error(E_NOTCONNECTED,
                "Command is unreachable because server is not connected");
    return;
  }

  _last_cmd = type;
  _last_tag = _tags->generate();

  _sock->write(QString{ "%1 %2%3" }.arg(_last_tag).arg(cmd).arg(CRLF).toUtf8());
  _sock->flush();
}

void
IMAP::_handle_login()
{
  auto resp = IMAPResponse{ _sock, _last_tag };

  if (resp.tagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged[0].first == Response::NO) {
    _trig_error(E_LOGIN, resp.tagged[0].second);
    return;
  }

  if (resp.tagged[0].first == Response::BAD) {
    _trig_error(E_BADCOMMAND, resp.tagged[0].second);
    return;
  }

  _queue.enqueue(QVariant::fromValue(response::Login{}));

  _status = S_AUTHENTICATE;
  qDebug() << "IMAP4 Client: Login successfully.";

  emit ready_read();
}

void
IMAP::_handle_logout()
{
  auto resp = IMAPResponse{ _sock, _last_tag };

  if (resp.tagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged[0].first != Response::OK) {
    _trig_error(E_BADCOMMAND, resp.tagged[0].second);
    return;
  }

  qDebug() << "IMAP4 Client: Logout successfully.";
}

void
IMAP::_handle_list()
{
  auto resp = IMAPResponse{ _sock, _last_tag };

  if (resp.tagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged[0].first == Response::NO) {
    _trig_error(E_REFERENCE, resp.tagged[0].second);
    return;
  }

  if (resp.tagged[0].first == Response::BAD) {
    _trig_error(E_BADCOMMAND, resp.tagged[0].second);
    return;
  }

  auto list_resp = response::List{};

  for (const auto& [type, data] : resp.untagged) {
    if (type != Response::LIST) {
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

  _queue.enqueue(QVariant::fromValue(list_resp));
  emit ready_read();
}

void
IMAP::_handle_select() // NOLINT
{
  auto resp = IMAPResponse{ _sock, _last_tag };

  if (resp.tagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged[0].first == Response::NO) {
    _trig_error(E_REFERENCE, resp.tagged[0].second);
    return;
  }

  if (resp.tagged[0].first == Response::BAD) {
    _trig_error(E_BADCOMMAND, resp.tagged[0].second);
    return;
  }

  auto select_resp = response::Select{};

  if (auto parsed = SELECT_BRACKET_REG.match(resp.tagged[0].second);
      parsed.hasMatch()) {
    select_resp.permission = parsed.captured("type");
  } else {
    qWarning()
      << "Failed to parse priority from SELECT response: Unexpected format."
      << resp.tagged[0].second;
  }

  for (const auto& item : resp.untagged_trailing) {
    if (item.first == Response::EXISTS) {
      bool ok = false;
      auto exists = item.second.toULongLong(&ok);
      if (!ok) {
        qWarning() << "Failed to parse SELECT EXISTS response: Not a number.";
        continue;
      }
      select_resp.exists = exists;
      continue;
    }

    if (item.first == Response::RECENT) {
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

  for (const auto& item : resp.untagged) {
    if (auto parsed = ATTRS_REG.match(item.second);
        item.first == Response::FLAGS && parsed.hasMatch()) {
      select_resp.flags = _parse_attrs(parsed.captured("attrs"));
    }

    if (auto parsed = SELECT_BRACKET_REG.match(item.second);
        item.first == Response::OK && parsed.hasMatch()) {
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

  _queue.enqueue(QVariant::fromValue(std::move(select_resp)));
  emit ready_read();
}

void
IMAP::_handle_noop()
{
  auto resp = IMAPResponse{ _sock, _last_tag };

  if (resp.tagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged[0].first != Response::OK) {
    _trig_error(E_BADCOMMAND, resp.tagged[0].second);
    return;
  }

  _queue.enqueue(QVariant::fromValue(response::Noop{}));
  emit ready_read();
}

void
IMAP::_handle_search()
{
  auto resp = IMAPResponse{ _sock, _last_tag };

  if (resp.tagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.tagged[0].first == Response::NO) {
    _trig_error(E_REFERENCE, resp.tagged[0].second);
    return;
  }

  if (resp.tagged[0].first == Response::BAD) {
    _trig_error(E_BADCOMMAND, resp.tagged[0].second);
    return;
  }

  if (resp.untagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected untagged response");
    return;
  }

  auto search_resp = QList<std::size_t>{};
  for (const auto& item :
       resp.untagged[0].second.split(' ', Qt::SkipEmptyParts)) {
    bool ok = false;
    auto index = item.toULongLong(&ok);
    if (!ok) {
      qWarning() << "Failed to parse SEARCH response: Not a number.";
      continue;
    }
    search_resp.push_back(index);
  }

  _queue.enqueue(QVariant::fromValue(std::move(search_resp)));
  emit ready_read();
}

QStringList
IMAP::_parse_attrs(const QString& attrs_str)
{
  auto attrs = attrs_str.split(' ', Qt::SkipEmptyParts);

  for (auto& item : attrs) {
    if (item.front() == '\\') {
      item.erase(item.begin());
    }
  }

  return attrs;
}

void
IMAP::_on_connected()
{
  if (!_sock->waitForReadyRead(TIMEOUT_MSECS)) {
    _trig_error(E_TCPINTERNAL);
    return;
  }

  auto resp = IMAPResponse{ _sock, "A000" };
  if (resp.untagged.size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.untagged[0].first == Response::OK) {
    _status = S_CONNECT;
  } else if (resp.untagged[0].first == Response::PREAUTH) {
    _status = S_AUTHENTICATE;
  } else {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  _tags = std::make_unique<TagGenerator>();
  connect(_sock, &QSslSocket::readyRead, this, &IMAP::_on_ready_read);
  connect(_sock, &QSslSocket::errorOccurred, this, &IMAP::_on_error_occurred);

  qInfo() << "IMAP4 Client: Connection established with tag" << _tags->label();

  emit connected();
}

void
IMAP::_on_disconnected()
{
  disconnect(
    _sock, &QSslSocket::errorOccurred, this, &IMAP::_on_error_occurred);
  disconnect(_sock, &QSslSocket::readyRead, this, &IMAP::_on_ready_read);
  _tags = nullptr;
  _status = S_DISCONNECT;

  qInfo() << "IMAP4 Client: Disconnected.";

  emit disconnected();
}

void
IMAP::_on_error_occurred(QSslSocket::SocketError /*error*/)
{
  _trig_error(E_TCPINTERNAL);
}

void
IMAP::_on_ready_read()
{
  if (_last_cmd == Command::LOGIN) {
    _handle_login();
    return;
  }

  if (_last_cmd == Command::LOGOUT) {
    _handle_logout();
    return;
  }

  if (_last_cmd == Command::LIST) {
    _handle_list();
    return;
  }

  if (_last_cmd == Command::SELECT) {
    _handle_select();
    return;
  }

  if (_last_cmd == Command::NOOP) {
    _handle_noop();
    return;
  }

  if (_last_cmd == Command::SEARCH) {
    _handle_search();
    return;
  }
}

}
