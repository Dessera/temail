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
IMAP::connect_to_host(const QString& url,
                      uint16_t port,
                      SslOption ssl,
                      const CommandCallback& callback)
{
  if (is_connected()) {
    _trig_error(E_DUPLICATE, "Connection has established");
    return;
  }

  if (port == 0) {
    port = ssl == USE_SSL ? PORT_USE_SSL : PORT_NO_SSL;
  }

  qDebug("IMAP4 Client: Try to connect to host %s:%d %s.",
         qPrintable(url),
         port,
         ssl == USE_SSL ? "with SSL" : "no SSL");

  _resp_callback.insert("CONNECT", callback);

  if (ssl == USE_SSL) {
    _sock->connectToHostEncrypted(url, port);
  } else {
    _sock->connectToHost(url, port);
  }
}

void
IMAP::disconnect_from_host(const CommandCallback& callback)
{
  if (_status == S_DISCONNECT) {
    _trig_error(E_DUPLICATE, "Connection has not established");
    return;
  }

  qDebug() << "IMAP4 Client: Try to disconnect from host.";

  _resp_callback.insert("DISCONNECT", callback);

  _sock->disconnectFromHost();
}

void
IMAP::login(const QString& username,
            const QString& password,
            const CommandCallback& callback)
{
  if (_status == S_AUTHENTICATE) {
    _trig_error(E_DUPLICATE, "Already authenticated");
    return;
  }

  _request(Command::LOGIN,
           QString{ "LOGIN %1 %2" }.arg(username).arg(password),
           callback);
}

void
IMAP::logout(const CommandCallback& callback)
{
  if (_status == S_CONNECT) {
    disconnect_from_host(callback);
    return;
  }

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
            request::Fetch::Field field,
            std::size_t range,
            const CommandCallback& callback)
{
  auto cmd_range = range <= 1 ? QString::number(id)
                              : QString{ "%1:%2" }.arg(id).arg(id + range - 1);

  _request(Command::FETCH,
           QString{ "FETCH %1 %2" }.arg(cmd_range).arg(FETCH_FIELD[field]),
           callback);
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
IMAP::_request(Command type,
               QAnyStringView cmd,
               const CommandCallback& callback)
{
  if (_status == S_DISCONNECT) {
    _trig_error(E_NOTCONNECTED, "Connection has not established");
    return;
  }

  auto tag = _tags->generate();

  _resp.emplace_back(type, std::make_unique<detail::IMAPResponse>(_sock, tag));

  if (!_resp_callback.contains(tag)) {
    _resp_callback.insert(tag, callback);
  } else {
    qWarning() << "Failed to add callback for command " << cmd
               << ": Callback already exists.";
  }

  _sock->write(QString{ "%1 %2%3" }.arg(tag).arg(cmd).arg(CRLF).toLocal8Bit());
  _sock->flush();
}

void
IMAP::_on_connected()
{
  if (!_sock->waitForReadyRead(TIMEOUT_MSECS)) {
    _trig_error(E_INTERNAL, _sock->errorString());
    return;
  }

  // assume that connect message must be sent at once.
  auto resp = detail::IMAPResponse{ _sock, "A000" };

  if (!resp.digest() || resp.untagged().size() != 1) {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  if (resp.untagged()[0].first == Response::OK) {
    _status = S_CONNECT;
  } else if (resp.untagged()[0].first == Response::PREAUTH) {
    _status = S_AUTHENTICATE;
  } else {
    _trig_error(E_UNEXPECTED, "Unexpected tagged response");
    return;
  }

  _tags = std::make_unique<TagGenerator>();
  connect(_sock, &QSslSocket::readyRead, this, &IMAP::_on_ready_read);
  connect(_sock, &QSslSocket::errorOccurred, this, &IMAP::_on_error_occurred);

  qInfo() << "IMAP4 Client: Connection established with tag" << _tags->label();

  if (_resp_callback.contains("CONNECT")) {
    _resp_callback["CONNECT"]({});
    _resp_callback.remove("CONNECT");
  }
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

  if (_resp_callback.contains("DISCONNECT")) {
    _resp_callback["DISCONNECT"]({});
    _resp_callback.remove("DISCONNECT");
  }
  emit disconnected();
}

void
IMAP::_on_error_occurred(QSslSocket::SocketError /*error*/)
{
  _trig_error(E_INTERNAL, _sock->errorString());
}

void
IMAP::_on_ready_read()
{
  if (_resp.empty()) {
    qWarning() << "IMAP4 Client: Unhandled response: " << _sock->readAll();
    return;
  }

  auto& resp = _resp.front();
  if (!resp.second->digest()) {
    if (resp.second->error()) {
      qWarning() << "IMAP4 Client: Failed to parse response for command "
                 << resp.first;

      _trig_error(E_PARSE, "Invalid response");
      _resp.erase(_resp.begin());
    }

    return;
  }

  RESPONSE_HANDLER[resp.first](
    resp.second.get(),
    [this](ErrorType err, const QString& estr) { _trig_error(err, estr); },
    [this, &resp](const QVariant& data) {
      if (resp.first == Command::LOGIN) {
        _status = S_AUTHENTICATE;
      }

      if (_resp_callback.contains(resp.second->tag())) {
        _resp_callback[resp.second->tag()](data);
        _resp_callback.remove(resp.second->tag());
      }

      if (resp.first == Command::LOGOUT) {
        return;
      }

      _queue.push_back(data);
      emit ready_read();
    });

  _resp.erase(_resp.begin());
}

}
