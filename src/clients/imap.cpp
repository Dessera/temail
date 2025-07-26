#include <cstdint>
#include <qdebug.h>
#include <qlist.h>
#include <qlogging.h>
#include <qmetaobject.h>
#include <qsslsocket.h>
#include <qstring.h>
#include <qtmetamacros.h>
#include <qtypes.h>
#include <utility>

#include "qvariant.h"
#include "temail/clients/imap.hpp"
#include "temail/common.hpp"
#include "temail/tag.hpp"

namespace temail::clients {

namespace {

/**
 * @brief IMAP4 response parser.
 *
 */
class IMAPResponse
{
public:
  /**
   * @brief IMAP4 response item (named pair).
   *
   * @tparam Te Key type.
   */
  template<typename Te>
  struct Item
  {
    Te type;      // NOLINT
    QString data; // NOLINT

    Item(Te item_type, QString item_data)
      : type{ std::move(item_type) }
      , data{ std::move(item_data) }
    {
    }
  };

  /**
   * @brief Regular expression to parse tagged response.
   *
   */
  inline static QRegularExpression TAGGED_REG{
    "(?P<tag>[A-Z]\\d+) (?P<type>[A-Z]+) (?P<data>.*)"
  };

  /**
   * @brief Regular expression to parse untagged response.
   *
   */
  inline static QRegularExpression UNTAGGED_REG{
    "\\* (?P<type>[A-Z-]+)( (?P<data>.*))?"
  };

private:
  QTextStream* _stream;

  QList<Item<IMAP::ResponseType>> _tagged_response;
  QList<Item<QString>> _untagged_response;

public:
  IMAPResponse(QTextStream* stream)
    : _stream{ stream }
  {
    auto buffer = QString{};
    while (_stream->readLineInto(&buffer)) {
      _handle_tagged(buffer);
      _handle_untagged(buffer);
    }
  }

  [[nodiscard]] TEMAIL_INLINE auto& tagged() const { return _tagged_response; }

  [[nodiscard]] TEMAIL_INLINE auto& tagged(qsizetype index) const
  {
    return _tagged_response[index];
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
  void _handle_tagged(const QString& buffer)
  {
    if (auto parsed = TAGGED_REG.match(buffer); parsed.hasMatch()) {
      _tagged_response.emplace_back(
        _str_to_enum<IMAP::ResponseType>(parsed.captured("type")),
        parsed.captured("data"));
    }
  }

  void _handle_untagged(const QString& buffer)
  {
    if (auto parsed = UNTAGGED_REG.match(buffer); parsed.hasMatch()) {
      _untagged_response.emplace_back(parsed.captured("type"),
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
    logout();
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
  if (_status == S_AUTHENTICATE) {
    return;
  }

  auto cmd = QString{ "%1 LOGIN %2 %3%4" }
               .arg(_tags->generate())
               .arg(username)
               .arg(password)
               .arg(CRLF);
  _send_command(LOGIN, cmd);
}

void
IMAP::logout()
{
  if (_status == S_CONNECT) {
    disconnect_from_host();
    return;
  }

  auto cmd = QString{ "%1 LOGOUT%2" }.arg(_tags->generate()).arg(CRLF);
  _send_command(LOGOUT, cmd);
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
IMAP::_send_command(CommandType type, const QString& cmd)
{
  if (_status == S_DISCONNECT) {
    _trig_error(E_NOTCONNECTED,
                "Command is unreachable because server is not connected");
    return;
  }

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
    _trig_error(E_UNEXPECTED, "Connect failed for unexpected response.");
    return;
  }

  if (resp.untagged(0).type == "OK") {
    _status = S_CONNECT;
  } else if (resp.untagged(0).type == "PREAUTH") {
    _status = S_AUTHENTICATE;
  } else {
    _trig_error(E_UNEXPECTED, "Connect failed for unexpected response.");
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

  if (_last_cmd == LOGIN) {
    if (resp.tagged().size() != 1) {
      _trig_error(E_UNEXPECTED, "Login failed for unexpected response.");
      return;
    }

    if (resp.tagged(0).type == NO) {
      _trig_error(E_LOGINFAIL, resp.tagged(0).data);
      return;
    }

    if (resp.tagged(0).type == BAD) {
      _trig_error(E_BADCOMMAND, resp.tagged(0).data);
      return;
    }

    _queue.enqueue(resp.tagged(0).data);

    _status = S_AUTHENTICATE;
    qDebug() << "IMAP4 Client: Login successfully.";
  } else if (_last_cmd == LOGOUT) {
    if (resp.tagged().size() != 1) {
      _trig_error(E_UNEXPECTED, "Logout failed for unexpected response.");
      return;
    }

    if (resp.tagged(0).type != OK) {
      _trig_error(E_BADCOMMAND, resp.tagged(0).data);
      return;
    }

    qDebug() << "IMAP4 Client: Logout successfully.";
    return;
  }

  emit ready_read();
}

}
