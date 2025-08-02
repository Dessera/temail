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

#include "temail/client/imap.hpp"
#include "temail/client/request.hpp"
#include "temail/common.hpp"
#include "temail/private/client/imap/fetch.hpp"
#include "temail/private/client/imap/list.hpp"
#include "temail/private/client/imap/login.hpp"
#include "temail/private/client/imap/logout.hpp"
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
  { IMAP::Command::SEARCH, detail::imap_handle_search },
  { IMAP::Command::FETCH, detail::imap_handle_fetch },
};

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

  return _error == E_NOERR;
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
    _trig_error(E_LOGIN, "Already authenticated");
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

// TODO: Remove if.
void
IMAP::fetch(std::size_t id, request::Fetch::Field field)
{
  if (field == request::Fetch::SIMPLE) {
    _send_command(Command::FETCH,
                  QString{ "FETCH %1 BODY.PEEK[HEADER]" }.arg(id));
  } else {
    _send_command(
      Command::FETCH,
      QString{ "FETCH %1 (BODY[HEADER.FIELDS (CONTENT-TYPE)] BODY[1])" }.arg(
        id));
  }
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

  auto tag = _tags->generate();

  _resp.emplace_back(type, std::make_unique<detail::IMAPResponse>(_sock, tag));

  _sock->write(QString{ "%1 %2%3" }.arg(tag).arg(cmd).arg(CRLF).toUtf8());
  _sock->flush();
}

void
IMAP::_on_connected()
{
  if (!_sock->waitForReadyRead(TIMEOUT_MSECS)) {
    _trig_error(E_TCPINTERNAL);
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
  if (_resp.empty()) {
    qWarning() << "IMAP4 Client: Unhandled response: " << _sock->readAll();
    return;
  }

  auto& resp = _resp.front();
  if (!resp.second->digest()) {
    if (resp.second->error()) {
      qWarning() << "IMAP4 Client: Failed to parse response for command "
                 << resp.first;

      _trig_error(E_PARSE, "Failed to parse response");
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

      _queue.push_back(data);
      emit ready_read();
    });

  _resp.erase(_resp.begin());
}

}
