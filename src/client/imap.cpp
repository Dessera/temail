#include <cstddef>
#include <cstdint>
#include <memory>
#include <qanystringview.h>
#include <qbytearray.h>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qmap.h>
#include <qmetaobject.h>
#include <qmutex.h>
#include <qpair.h>
#include <qregularexpression.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <qvariant.h>
#include <utility>

#include "temail/client/base.hpp"
#include "temail/client/imap.hpp"
#include "temail/client/request.hpp"
#include "temail/common.hpp"
#include "temail/private/client/imap/fetch.hpp"
#include "temail/private/client/imap/list.hpp"
#include "temail/private/client/imap/login.hpp"
#include "temail/private/client/imap/logout.hpp"
#include "temail/private/client/imap/noop.hpp"
#include "temail/private/client/imap/response.hpp"
#include "temail/private/client/imap/search.hpp"
#include "temail/private/client/imap/select.hpp"
#include "temail/tag.hpp"

namespace temail::client {

const QMap<IMAP::Command, IMAP::ResponseHandler> IMAP::RESPONSE_HANDLER{
  { IMAP::Command::LOGIN, detail::imap_handle_login },
  { IMAP::Command::LOGOUT, detail::imap_handle_logout },
  { IMAP::Command::LIST, detail::imap_handle_list },
  { IMAP::Command::SELECT, detail::imap_handle_select },
  { IMAP::Command::NOOP, detail::imap_handle_noop },
  { IMAP::Command::SEARCH, detail::imap_handle_search },
  { IMAP::Command::FETCH, detail::imap_handle_fetch },
};

IMAP::IMAP(QObject* parent)
  : Base{ parent }
{
  connect(&_sock, &QSslSocket::connected, this, &IMAP::_on_connected);
  connect(&_sock, &QSslSocket::disconnected, this, &IMAP::_on_disconnected);
}

IMAP::~IMAP()
{
  if (is_connected()) {
    logout();
    wait_for_disconnected();
  }
}

void
IMAP::connect_to_host(const QString& url,
                      uint16_t port,
                      SslOption ssl,
                      const CommandCallback& callback)
{
  QMutexLocker request_guard{ &_request_lock };

  _add_handler(CONNECT_TAG, callback, _default_error_handler);

  if (is_connected()) {
    _tag_error(CONNECT_TAG, E_DUPLICATE, "Connection has established");
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
    _sock.connectToHostEncrypted(url, port);
  } else {
    _sock.connectToHost(url, port);
  }
}

void
IMAP::disconnect_from_host(const CommandCallback& callback)
{
  QMutexLocker request_guard{ &_request_lock };

  _add_handler(DISCONNECT_TAG, callback, _default_error_handler);

  if (is_disconnected()) {
    _tag_error(DISCONNECT_TAG, E_DUPLICATE, "Connection has not established");
    return;
  }

  qDebug() << "IMAP4 Client: Try to disconnect from host.";

  _sock.disconnectFromHost();
}

void
IMAP::login(const QString& username,
            const QString& password,
            const CommandCallback& callback)
{
  _request(Command::LOGIN,
           QString{ "LOGIN %1 %2" }.arg(username).arg(password),
           callback);
}

void
IMAP::logout(const CommandCallback& callback)
{
  _request(Command::LOGOUT, "LOGOUT", callback);
}

void
IMAP::list(const QString& path,
           const QString& pattern,
           const CommandCallback& callback)
{
  _request(
    Command::LIST, QString{ "LIST %2 %3" }.arg(path).arg(pattern), callback);
}

void
IMAP::select(const QString& path, const CommandCallback& callback)
{
  _request(Command::SELECT, QString{ "SELECT %2" }.arg(path), callback);
}

void
IMAP::noop(const CommandCallback& callback)
{
  _request(Command::NOOP, "NOOP", callback);
}

void
IMAP::search(request::Search::Criteria criteria,
             const CommandCallback& callback)
{
  _request(Command::SEARCH,
           QString{ "SEARCH %1" }.arg(common::enum_name(criteria)),
           callback);
}

void
IMAP::fetch(std::size_t id,
            request::Fetch::FieldFlags field,
            std::size_t range,
            const CommandCallback& callback)
{
  auto cmd_range = range <= 1 ? QString::number(id)
                              : QString{ "%1:%2" }.arg(id).arg(id + range - 1);

  auto cmd_fields = QString{};
  if (field.testFlag(request::Fetch::ENVELOPE)) {
    cmd_fields.append(FETCH_FIELD[request::Fetch::ENVELOPE]);
    cmd_fields.append(' ');
  }

  if (field.testFlag(request::Fetch::MIME)) {
    cmd_fields.append(FETCH_FIELD[request::Fetch::MIME]);
    cmd_fields.append(' ');
  }

  if (field.testFlag(request::Fetch::TEXT)) {
    cmd_fields.append(FETCH_FIELD[request::Fetch::TEXT]);
    cmd_fields.append(' ');
  }

  _request(Command::FETCH,
           QString{ "FETCH %1 (%2)" }.arg(cmd_range).arg(cmd_fields),
           callback);
}

QVariant
IMAP::read()
{
  QMutexLocker guard{ &_read_lock };

  if (_queue.size() == 0) {
    qWarning() << "Failed to read from IMAP client: No response in queue.";
    return {};
  }

  auto data = _queue.front();
  _queue.pop();

  return data;
}

void
IMAP::_request(Command type,
               QAnyStringView cmd,
               const CommandCallback& callback)
{
  QMutexLocker request_guard{ &_request_lock };

  auto tag = _tags.generate();
  _add_handler(tag, callback, _default_error_handler);

  if (_status == Status::DISCONNECT) {
    _tag_error(tag, E_NOTCONNECTED, "Connection has not established");
    return;
  }

  QMutexLocker guard{ &_resp_lock };
  _resp.emplace_back(type, detail::IMAPResponse{ tag });
  guard.unlock();

  _sock.write(QString{ "%1 %2\r\n" }.arg(tag).arg(cmd).toLocal8Bit());
  if (!_sock.flush()) {
    // there must be one response.
    guard.relock();
    _resp.pop_back();

    _tag_error(tag, E_INTERNAL, _sock.errorString());
  }
}

void
IMAP::_tag_error(const QString& tag, ErrorType error, const QString& estr)
{
  _handle_error(tag, error, estr);
  _set_error(error, estr);
}

void
IMAP::_handle_success(const QString& tag, const QVariant& data)
{
  CommandCallback cb;
  QMutexLocker guard{ &_cb_lock };

  if (!_resp_cb.contains(tag)) {
    return;
  }
  cb = _resp_cb[tag].first;
  _resp_cb.remove(tag);
  guard.unlock();

  cb(data);
}

void
IMAP::_handle_error(const QString& tag, ErrorType error, const QString& estr)
{
  ErrorCallback cb;
  QMutexLocker guard{ &_cb_lock };

  if (!_resp_cb.contains(tag)) {
    return;
  }
  cb = _resp_cb[tag].second;
  _resp_cb.remove(tag);
  guard.unlock();

  cb(error, estr);
}

void
IMAP::_add_handler(const QString& tag,
                   const CommandCallback& success,
                   const ErrorCallback& error)
{
  QMutexLocker guard{ &_cb_lock };
  _resp_cb.insert(tag, { success, error });
}

void
IMAP::_on_connected()
{
  if (!_sock.waitForReadyRead(TIMEOUT_MSECS)) {
    _tag_error(CONNECT_TAG, E_INTERNAL, _sock.errorString());
    return;
  }

  // * assume that connect message must be sent at once.
  auto resp = detail::IMAPResponse{ CONNECT_TAG };

  if (!resp.digest(_sock.readAll()) || resp.untagged().size() != 1) {
    _tag_error(CONNECT_TAG, E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.untagged()[0].first == Response::OK) {
    _status = Status::CONNECT;
  } else if (resp.untagged()[0].first == Response::PREAUTH) {
    _status = Status::AUTHENTICATE;
  } else {
    _tag_error(CONNECT_TAG, E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  connect(&_sock, &QSslSocket::readyRead, this, &IMAP::_on_ready_read);
  connect(&_sock, &QSslSocket::errorOccurred, this, &IMAP::_on_error_occurred);

  qInfo() << "IMAP4 Client: Connection established with tag" << _tags.label();

  _handle_success(CONNECT_TAG, {});
  emit connected();
}

void
IMAP::_on_disconnected()
{
  disconnect(
    &_sock, &QSslSocket::errorOccurred, this, &IMAP::_on_error_occurred);
  disconnect(&_sock, &QSslSocket::readyRead, this, &IMAP::_on_ready_read);
  _status = Status::DISCONNECT;

  qInfo() << "IMAP4 Client: Disconnected.";

  _handle_success(DISCONNECT_TAG, {});
  emit disconnected();
}

void
IMAP::_on_error_occurred(QSslSocket::SocketError /*error*/)
{
  QMutexLocker guard{ &_resp_lock };
  if (_resp.empty()) {
    _set_error(E_INTERNAL, _sock.errorString());
    return;
  }

  const auto& tag = _resp.front().second.tag();
  _tag_error(tag, E_INTERNAL, _sock.errorString());

  _resp.pop_front();
}

void
IMAP::_on_ready_read()
{
  // read all response immediately.
  auto data = _sock.readAll();

  // get first parser (there must be at least one response)
  QMutexLocker guard{ &_resp_lock };
  if (_resp.empty()) {
    qWarning() << "IMAP4 Client: Unhandled response: " << data;
    return;
  }

  auto& resp = _resp.front();

  auto state = resp.second.digest(data);
  auto error = resp.second.error();

  // Not a complete response.
  if (!state && !error) {
    return;
  }

  if (state && !error) {
    // Response finished with success
    RESPONSE_HANDLER[resp.first](
      resp.second,

      // Parse error
      [this, &resp](ErrorType err, const QString& estr) {
        _tag_error(resp.second.tag(), err, estr);
      },

      // Parse success
      [this, &resp](const QVariant& data) {
        if (resp.first == Command::LOGIN) {
          _status = Status::AUTHENTICATE;
        }

        _handle_success(resp.second.tag(), data);

        _read_lock.lock();
        _queue.push(data);
        _read_lock.unlock();

        emit ready_read();
      });
  } else {
    // Response finished with error.
    qWarning() << "IMAP4 Client: Failed to parse response for command "
               << resp.first;
    _tag_error(resp.second.tag(), E_PARSE, "Invalid response");
  }

  _resp.pop_front();
}

}
