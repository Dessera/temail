#include <cstdint>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qmetaobject.h>
#include <qpair.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qtmetamacros.h>
#include <qtypes.h>

#include "temail/clients/imap.hpp"
#include "temail/common.hpp"
#include "temail/tag.hpp"

namespace temail::clients {

namespace {

class IMAPResponse
{
public:
  inline static QRegularExpression UNTAGGED_REG{
    "\\* (?P<type>[A-Z-]+)( (?P<data>.*))?"
  };

private:
  QTextStream* _stream;

  QList<QPair<IMAP::ResponseType, QString>> _untagged_response;

public:
  IMAPResponse(QTextStream* stream)
    : _stream{ stream }
  {
    auto buffer = QString{};
    while (_stream->readLineInto(&buffer)) {
      _handle_untagged(buffer);
    }
  }

  [[nodiscard]] TEMAIL_INLINE auto& untagged() const
  {
    return _untagged_response;
  }

  [[nodiscard]] TEMAIL_INLINE auto& untagged(qsizetype index) const
  {
    return _untagged_response[index];
  }

private:
  void _handle_untagged(const QString& buffer)
  {
    if (auto parsed = UNTAGGED_REG.match(buffer); parsed.hasMatch()) {
      _untagged_response.emplace_back(
        _str_to_enum<IMAP::ResponseType>(parsed.captured("type")),
        parsed.captured("data"));
    }
  }

  template<typename Em>
  Em _str_to_enum(const QString& name)
  {
    auto enum_meta = QMetaEnum::fromType<Em>();
    return static_cast<Em>(enum_meta.keysToValue(name.toStdString().c_str()));
  }
};

}

IMAP::IMAP(QObject* parent)
  : QObject{ parent }
  , _sock{ new QSslSocket{ this } }
  , _stream{ _sock }
{
  _stream.setEncoding(QStringConverter::Utf8);

  connect(_sock, &QSslSocket::connected, this, &IMAP::_on_connected);
  connect(_sock, &QSslSocket::disconnected, this, &IMAP::_on_disconnected);
}

IMAP::~IMAP()
{
  if (is_connected()) {
    disconnect_from_host();
    wait_for_disconnected();
  }
}

void
IMAP::connect_to_host(const QString& url, uint16_t port, SslFlags ssl)
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
IMAP::connect_to_host(const QString& url, SslFlags ssl)
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
  auto cmd = QString{ "%1 LOGIN %2 %3%4" }
               .arg(_tags->generate())
               .arg(username)
               .arg(password)
               .arg(CRLF);

  _send_command(LOGIN, cmd);
}

void
IMAP::list(const QString& path, const QString& pattern)
{
  auto cmd = QString{ "%1 LIST %2 %3%4" }
               .arg(_tags->generate())
               .arg(path)
               .arg(pattern)
               .arg(CRLF);

  _send_command(LIST, cmd);
}

void
IMAP::_send_command(CommandType type, const QString& cmd)
{
  _last_cmd = type;

  _sock->write(cmd.toUtf8());
  _sock->flush();
}

void
IMAP::_on_connected()
{
  if (!_sock->waitForReadyRead(TIMEOUT_MSECS)) {
    _trig_error(E_TCPINTERNAL);
    return;
  }

  auto resp = IMAPResponse{ &_stream };
  if (resp.untagged().size() != 1) {
    _trig_error(E_TCPINTERNAL);
    return;
  }

  if (resp.untagged(0).first == OK) {
    _status = S_CONNECT;
  } else if (resp.untagged(0).first == PREAUTH) {
    _status = S_AUTHENTICATE;
  } else {
    _trig_error(E_UNEXPECTED_STATUS, "Unexpected Status when connecting");
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
  auto resp = IMAPResponse{ &_stream };

  emit ready_read();
}

}
